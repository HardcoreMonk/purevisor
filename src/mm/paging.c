/*
 * PureVisor - Paging Implementation
 * 
 * x86_64 4-level paging management
 */

#include <lib/types.h>
#include <lib/string.h>
#include <mm/paging.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static vm_context_t kernel_context;
static bool paging_initialized = false;

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

/* Convert flags to PTE bits */
static uint64_t flags_to_pte(uint32_t flags)
{
    uint64_t pte_flags = PTE_PRESENT;
    
    if (flags & MAP_WRITE) pte_flags |= PTE_WRITABLE;
    if (flags & MAP_USER) pte_flags |= PTE_USER;
    if (!(flags & MAP_EXEC)) pte_flags |= PTE_NO_EXECUTE;
    if (flags & MAP_NOCACHE) pte_flags |= PTE_CACHE_DISABLE;
    
    return pte_flags;
}

/* Allocate a page table */
static pte_t *alloc_page_table(void)
{
    phys_addr_t phys = pmm_alloc_page();
    if (!phys) return NULL;
    
    pte_t *table = (pte_t *)phys_to_virt(phys);
    memset(table, 0, PAGE_SIZE);
    return table;
}

/* Free a page table */
static void free_page_table(pte_t *table) UNUSED;
static void free_page_table(pte_t *table)
{
    phys_addr_t phys = virt_to_phys(table);
    pmm_free_page(phys);
}

/* Get or create page table entry */
static pte_t *get_pte(pml4_t pml4, virt_addr_t virt, bool create)
{
    uint64_t pml4_idx = PML4_INDEX(virt);
    uint64_t pdpt_idx = PDPT_INDEX(virt);
    uint64_t pd_idx = PD_INDEX(virt);
    uint64_t pt_idx = PT_INDEX(virt);
    
    /* PML4 -> PDPT */
    pte_t *pml4_entry = &pml4[pml4_idx];
    pdpt_t pdpt;
    
    if (*pml4_entry & PTE_PRESENT) {
        pdpt = (pdpt_t)phys_to_virt(*pml4_entry & PTE_ADDR_MASK);
    } else {
        if (!create) return NULL;
        pdpt = (pdpt_t)alloc_page_table();
        if (!pdpt) return NULL;
        *pml4_entry = virt_to_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE;
    }
    
    /* PDPT -> PD */
    pte_t *pdpt_entry = &pdpt[pdpt_idx];
    pd_t pd;
    
    if (*pdpt_entry & PTE_PRESENT) {
        /* Check for 1GB huge page */
        if (*pdpt_entry & PTE_HUGE) return pdpt_entry;
        pd = (pd_t)phys_to_virt(*pdpt_entry & PTE_ADDR_MASK);
    } else {
        if (!create) return NULL;
        pd = (pd_t)alloc_page_table();
        if (!pd) return NULL;
        *pdpt_entry = virt_to_phys(pd) | PTE_PRESENT | PTE_WRITABLE;
    }
    
    /* PD -> PT */
    pte_t *pd_entry = &pd[pd_idx];
    pt_t pt;
    
    if (*pd_entry & PTE_PRESENT) {
        /* Check for 2MB huge page */
        if (*pd_entry & PTE_HUGE) return pd_entry;
        pt = (pt_t)phys_to_virt(*pd_entry & PTE_ADDR_MASK);
    } else {
        if (!create) return NULL;
        pt = (pt_t)alloc_page_table();
        if (!pt) return NULL;
        *pd_entry = virt_to_phys(pt) | PTE_PRESENT | PTE_WRITABLE;
    }
    
    return &pt[pt_idx];
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void paging_init(void)
{
    pr_info("Paging: Initializing...");
    
    /* Get current CR3 (set up by boot code) */
    kernel_context.pml4_phys = read_cr3() & PTE_ADDR_MASK;
    kernel_context.pml4 = (pml4_t)phys_to_virt(kernel_context.pml4_phys);
    kernel_context.flags = 0;
    
    paging_initialized = true;
    
    pr_info("Paging: Kernel PML4 at 0x%llx", kernel_context.pml4_phys);
}

vm_context_t *paging_create_context(void)
{
    vm_context_t *ctx = (vm_context_t *)kmalloc(sizeof(vm_context_t), GFP_KERNEL);
    if (!ctx) return NULL;
    
    /* Allocate PML4 */
    phys_addr_t pml4_phys = pmm_alloc_page();
    if (!pml4_phys) {
        kfree(ctx);
        return NULL;
    }
    
    ctx->pml4_phys = pml4_phys;
    ctx->pml4 = (pml4_t)phys_to_virt(pml4_phys);
    ctx->flags = 0;
    
    /* Clear user space, copy kernel space from kernel context */
    memset(ctx->pml4, 0, PAGE_SIZE / 2);  /* Clear lower half */
    memcpy(&ctx->pml4[256], &kernel_context.pml4[256], PAGE_SIZE / 2);  /* Copy higher half */
    
    return ctx;
}

void paging_destroy_context(vm_context_t *ctx)
{
    if (!ctx || ctx == &kernel_context) return;
    
    /* Free user page tables recursively
     * Walk through PML4 -> PDPT -> PD -> PT hierarchy
     * Only free user-space entries (lower half of address space)
     */
    pte_t *pml4 = ctx->pml4;
    
    /* Only iterate user-space (entries 0-255, lower half) */
    for (int i = 0; i < 256; i++) {
        if (!(pml4[i] & PTE_PRESENT)) continue;
        
        pte_t *pdpt = (pte_t *)phys_to_virt(pml4[i] & PTE_ADDR_MASK);
        
        for (int j = 0; j < 512; j++) {
            if (!(pdpt[j] & PTE_PRESENT)) continue;
            if (pdpt[j] & PTE_HUGE) continue;  /* 1GB page, no PT */
            
            pte_t *pd = (pte_t *)phys_to_virt(pdpt[j] & PTE_ADDR_MASK);
            
            for (int k = 0; k < 512; k++) {
                if (!(pd[k] & PTE_PRESENT)) continue;
                if (pd[k] & PTE_HUGE) continue;  /* 2MB page, no PT */
                
                /* Free page table */
                phys_addr_t pt_phys = pd[k] & PTE_ADDR_MASK;
                pmm_free_page(pt_phys);
            }
            
            /* Free page directory */
            phys_addr_t pd_phys = pdpt[j] & PTE_ADDR_MASK;
            pmm_free_page(pd_phys);
        }
        
        /* Free PDPT */
        phys_addr_t pdpt_phys = pml4[i] & PTE_ADDR_MASK;
        pmm_free_page(pdpt_phys);
    }
    
    /* Free PML4 */
    pmm_free_page(ctx->pml4_phys);
    kfree(ctx);
}

void paging_switch_context(vm_context_t *ctx)
{
    if (!ctx) ctx = &kernel_context;
    write_cr3(ctx->pml4_phys);
}

int paging_map(vm_context_t *ctx, virt_addr_t virt, phys_addr_t phys,
               size_t size, uint32_t flags)
{
    if (!ctx) ctx = &kernel_context;
    
    uint64_t pte_flags = flags_to_pte(flags);
    
    /* Handle huge pages */
    if (flags & MAP_HUGE_1G) {
        /* 1GB pages */
        while (size >= GB) {
            pte_t *pdpt_entry = get_pte(ctx->pml4, virt, true);
            if (!pdpt_entry) return -1;
            
            /* Go back to PDPT level */
            uint64_t pml4_idx = PML4_INDEX(virt);
            uint64_t pdpt_idx = PDPT_INDEX(virt);
            pdpt_t pdpt = (pdpt_t)phys_to_virt(ctx->pml4[pml4_idx] & PTE_ADDR_MASK);
            pdpt[pdpt_idx] = phys | pte_flags | PTE_HUGE;
            
            virt += GB;
            phys += GB;
            size -= GB;
        }
    }
    
    if (flags & MAP_HUGE_2M) {
        /* 2MB pages */
        while (size >= 2 * MB) {
            uint64_t pml4_idx = PML4_INDEX(virt);
            uint64_t pdpt_idx = PDPT_INDEX(virt);
            uint64_t pd_idx = PD_INDEX(virt);
            
            /* Ensure PML4 and PDPT entries exist */
            if (!(ctx->pml4[pml4_idx] & PTE_PRESENT)) {
                pdpt_t pdpt = (pdpt_t)alloc_page_table();
                if (!pdpt) return -1;
                ctx->pml4[pml4_idx] = virt_to_phys(pdpt) | PTE_PRESENT | PTE_WRITABLE;
            }
            
            pdpt_t pdpt = (pdpt_t)phys_to_virt(ctx->pml4[pml4_idx] & PTE_ADDR_MASK);
            if (!(pdpt[pdpt_idx] & PTE_PRESENT)) {
                pd_t pd = (pd_t)alloc_page_table();
                if (!pd) return -1;
                pdpt[pdpt_idx] = virt_to_phys(pd) | PTE_PRESENT | PTE_WRITABLE;
            }
            
            pd_t pd = (pd_t)phys_to_virt(pdpt[pdpt_idx] & PTE_ADDR_MASK);
            pd[pd_idx] = phys | pte_flags | PTE_HUGE;
            
            virt += 2 * MB;
            phys += 2 * MB;
            size -= 2 * MB;
        }
    }
    
    /* 4KB pages for remaining */
    while (size > 0) {
        pte_t *pte = get_pte(ctx->pml4, virt, true);
        if (!pte) return -1;
        
        *pte = phys | pte_flags;
        paging_flush_tlb(virt);
        
        virt += PAGE_SIZE;
        phys += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    
    return 0;
}

int paging_unmap(vm_context_t *ctx, virt_addr_t virt, size_t size)
{
    if (!ctx) ctx = &kernel_context;
    
    while (size > 0) {
        pte_t *pte = get_pte(ctx->pml4, virt, false);
        if (pte && (*pte & PTE_PRESENT)) {
            *pte = 0;
            paging_flush_tlb(virt);
        }
        
        virt += PAGE_SIZE;
        if (size >= PAGE_SIZE) size -= PAGE_SIZE;
        else size = 0;
    }
    
    return 0;
}

phys_addr_t paging_get_phys(vm_context_t *ctx, virt_addr_t virt)
{
    if (!ctx) ctx = &kernel_context;
    
    pte_t *pte = get_pte(ctx->pml4, virt, false);
    if (!pte || !(*pte & PTE_PRESENT)) return 0;
    
    return (*pte & PTE_ADDR_MASK) | PAGE_OFFSET(virt);
}

vm_context_t *paging_get_kernel_context(void)
{
    return &kernel_context;
}
