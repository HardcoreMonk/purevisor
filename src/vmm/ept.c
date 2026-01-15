/*
 * PureVisor - EPT (Extended Page Tables) Implementation
 * 
 * Intel EPT for guest physical address translation
 */

#include <lib/types.h>
#include <lib/string.h>
#include <vmm/ept.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static ept_entry_t *ept_alloc_table(void)
{
    phys_addr_t phys = pmm_alloc_page();
    if (!phys) return NULL;
    
    ept_entry_t *table = (ept_entry_t *)phys_to_virt(phys);
    memset(table, 0, PAGE_SIZE);
    return table;
}

static void ept_free_table(ept_entry_t *table)
{
    if (table) {
        phys_addr_t phys = virt_to_phys(table);
        pmm_free_page(phys);
    }
}

static ept_entry_t *ept_walk(ept_context_t *ept, phys_addr_t guest_phys,
                                   bool create, int *out_level)
{
    ept_entry_t *pml4 = ept->pml4;
    int pml4_idx = EPT_PML4_INDEX(guest_phys);
    int pdpt_idx = EPT_PDPT_INDEX(guest_phys);
    int pd_idx = EPT_PD_INDEX(guest_phys);
    int pt_idx = EPT_PT_INDEX(guest_phys);
    
    /* PML4 -> PDPT */
    ept_entry_t *pml4_entry = &pml4[pml4_idx];
    ept_entry_t *pdpt;
    
    if (*pml4_entry & EPT_READ) {
        pdpt = (ept_entry_t *)phys_to_virt(*pml4_entry & EPT_ADDR_MASK);
    } else {
        if (!create) return NULL;
        pdpt = ept_alloc_table();
        if (!pdpt) return NULL;
        *pml4_entry = virt_to_phys(pdpt) | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    }
    
    /* PDPT -> PD */
    ept_entry_t *pdpt_entry = &pdpt[pdpt_idx];
    
    if (*pdpt_entry & EPT_LARGE_PAGE) {
        if (out_level) *out_level = 3;
        return pdpt_entry;
    }
    
    ept_entry_t *pd;
    if (*pdpt_entry & EPT_READ) {
        pd = (ept_entry_t *)phys_to_virt(*pdpt_entry & EPT_ADDR_MASK);
    } else {
        if (!create) return NULL;
        pd = ept_alloc_table();
        if (!pd) return NULL;
        *pdpt_entry = virt_to_phys(pd) | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    }
    
    /* PD -> PT */
    ept_entry_t *pd_entry = &pd[pd_idx];
    
    if (*pd_entry & EPT_LARGE_PAGE) {
        if (out_level) *out_level = 2;
        return pd_entry;
    }
    
    ept_entry_t *pt;
    if (*pd_entry & EPT_READ) {
        pt = (ept_entry_t *)phys_to_virt(*pd_entry & EPT_ADDR_MASK);
    } else {
        if (!create) return NULL;
        pt = ept_alloc_table();
        if (!pt) return NULL;
        *pd_entry = virt_to_phys(pt) | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    }
    
    if (out_level) *out_level = 1;
    return &pt[pt_idx];
}

/* ============================================================================
 * Public API
 * ============================================================================ */

ept_context_t *ept_create(void)
{
    ept_context_t *ept = kmalloc(sizeof(ept_context_t), GFP_KERNEL | GFP_ZERO);
    if (!ept) {
        pr_error("EPT: Failed to allocate context");
        return NULL;
    }
    
    ept->pml4 = ept_alloc_table();
    if (!ept->pml4) {
        pr_error("EPT: Failed to allocate PML4");
        kfree(ept);
        return NULL;
    }
    
    ept->pml4_phys = virt_to_phys(ept->pml4);
    ept->eptp = ept->pml4_phys | EPT_MEMTYPE_WB | EPT_PAGE_WALK_4;
    
    pr_info("EPT: Created, PML4=0x%llx", ept->pml4_phys);
    return ept;
}

void ept_destroy(ept_context_t *ept)
{
    if (!ept) return;
    
    /* Free PML4 (simplified - should free all tables) */
    if (ept->pml4) {
        ept_free_table(ept->pml4);
    }
    kfree(ept);
}

int ept_map_page(ept_context_t *ept, phys_addr_t guest_phys,
                 phys_addr_t host_phys, uint32_t perm, uint32_t memtype)
{
    if (!ept) return -1;
    
    guest_phys &= ~(PAGE_SIZE - 1);
    host_phys &= ~(PAGE_SIZE - 1);
    
    ept_entry_t *entry = ept_walk(ept, guest_phys, true, NULL);
    if (!entry) return -1;
    
    *entry = host_phys | perm | ((uint64_t)memtype << EPT_MEMTYPE_SHIFT);
    ept->mapped_pages++;
    
    return 0;
}

int ept_map_page_2m(ept_context_t *ept, phys_addr_t guest_phys,
                    phys_addr_t host_phys, uint32_t perm, uint32_t memtype)
{
    if (!ept) return -1;
    
    guest_phys &= ~(2 * MB - 1);
    host_phys &= ~(2 * MB - 1);
    
    ept_entry_t *pml4 = ept->pml4;
    int pml4_idx = EPT_PML4_INDEX(guest_phys);
    int pdpt_idx = EPT_PDPT_INDEX(guest_phys);
    int pd_idx = EPT_PD_INDEX(guest_phys);
    
    /* Ensure PML4 -> PDPT */
    if (!(pml4[pml4_idx] & EPT_READ)) {
        ept_entry_t *pdpt = ept_alloc_table();
        if (!pdpt) return -1;
        pml4[pml4_idx] = virt_to_phys(pdpt) | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    }
    
    ept_entry_t *pdpt = (ept_entry_t *)phys_to_virt(pml4[pml4_idx] & EPT_ADDR_MASK);
    
    /* Ensure PDPT -> PD */
    if (!(pdpt[pdpt_idx] & EPT_READ)) {
        ept_entry_t *pd = ept_alloc_table();
        if (!pd) return -1;
        pdpt[pdpt_idx] = virt_to_phys(pd) | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    }
    
    ept_entry_t *pd = (ept_entry_t *)phys_to_virt(pdpt[pdpt_idx] & EPT_ADDR_MASK);
    
    /* Set 2MB page entry */
    pd[pd_idx] = host_phys | perm | EPT_LARGE_PAGE | 
                 ((uint64_t)memtype << EPT_MEMTYPE_SHIFT);
    
    ept->mapped_pages += 512;
    return 0;
}

int ept_map_page_1g(ept_context_t *ept, phys_addr_t guest_phys,
                    phys_addr_t host_phys, uint32_t perm, uint32_t memtype)
{
    if (!ept) return -1;
    
    guest_phys &= ~(GB - 1);
    host_phys &= ~(GB - 1);
    
    ept_entry_t *pml4 = ept->pml4;
    int pml4_idx = EPT_PML4_INDEX(guest_phys);
    int pdpt_idx = EPT_PDPT_INDEX(guest_phys);
    
    /* Ensure PML4 -> PDPT */
    if (!(pml4[pml4_idx] & EPT_READ)) {
        ept_entry_t *pdpt = ept_alloc_table();
        if (!pdpt) return -1;
        pml4[pml4_idx] = virt_to_phys(pdpt) | EPT_READ | EPT_WRITE | EPT_EXECUTE;
    }
    
    ept_entry_t *pdpt = (ept_entry_t *)phys_to_virt(pml4[pml4_idx] & EPT_ADDR_MASK);
    
    /* Set 1GB page entry */
    pdpt[pdpt_idx] = host_phys | perm | EPT_LARGE_PAGE |
                     ((uint64_t)memtype << EPT_MEMTYPE_SHIFT);
    
    ept->mapped_pages += 512 * 512;
    return 0;
}

int ept_unmap_page(ept_context_t *ept, phys_addr_t guest_phys)
{
    if (!ept) return -1;
    
    ept_entry_t *entry = ept_walk(ept, guest_phys, false, NULL);
    if (entry && (*entry & EPT_READ)) {
        *entry = 0;
        ept->mapped_pages--;
        return 0;
    }
    return -1;
}

int ept_map_range(ept_context_t *ept, phys_addr_t guest_phys,
                  phys_addr_t host_phys, uint64_t size,
                  uint32_t perm, uint32_t memtype)
{
    if (!ept || size == 0) return -1;
    
    pr_info("EPT: Mapping range GPA=0x%llx -> HPA=0x%llx, size=%llu MB",
            guest_phys, host_phys, size / MB);
    
    /* Use 2MB pages for aligned, large ranges */
    while (size >= 2 * MB && 
           (guest_phys & (2 * MB - 1)) == 0 && 
           (host_phys & (2 * MB - 1)) == 0) {
        if (ept_map_page_2m(ept, guest_phys, host_phys, perm, memtype) != 0) {
            return -1;
        }
        guest_phys += 2 * MB;
        host_phys += 2 * MB;
        size -= 2 * MB;
    }
    
    /* Use 4KB pages for remainder */
    while (size >= PAGE_SIZE) {
        if (ept_map_page(ept, guest_phys, host_phys, perm, memtype) != 0) {
            return -1;
        }
        guest_phys += PAGE_SIZE;
        host_phys += PAGE_SIZE;
        size -= PAGE_SIZE;
    }
    
    return 0;
}

int ept_set_permissions(ept_context_t *ept, phys_addr_t guest_phys, uint32_t perm)
{
    if (!ept) return -1;
    
    ept_entry_t *entry = ept_walk(ept, guest_phys, false, NULL);
    if (!entry) return -1;
    
    /* Clear old permissions, set new ones */
    *entry = (*entry & ~(EPT_READ | EPT_WRITE | EPT_EXECUTE)) | perm;
    return 0;
}

ept_entry_t ept_get_entry_value(ept_context_t *ept, phys_addr_t guest_phys)
{
    if (!ept) return 0;
    
    ept_entry_t *entry = ept_walk(ept, guest_phys, false, NULL);
    return entry ? *entry : 0;
}

phys_addr_t ept_get_host_phys(ept_context_t *ept, phys_addr_t guest_phys)
{
    if (!ept) return 0;
    
    int level;
    ept_entry_t *entry = ept_walk(ept, guest_phys, false, &level);
    if (!entry || !(*entry & EPT_READ)) return 0;
    
    phys_addr_t base = *entry & EPT_ADDR_MASK;
    
    switch (level) {
        case 3:  /* 1GB page */
            return base | (guest_phys & (GB - 1));
        case 2:  /* 2MB page */
            return base | (guest_phys & (2 * MB - 1));
        default: /* 4KB page */
            return base | (guest_phys & (PAGE_SIZE - 1));
    }
}

int ept_handle_violation(ept_context_t *ept, ept_violation_t *violation)
{
    if (!ept || !violation) return -1;
    
    pr_warn("EPT: Violation at GPA=0x%llx (R=%d W=%d X=%d)",
            violation->guest_phys,
            violation->read, violation->write, violation->execute);
    
    /* For now, just report - actual handling depends on use case */
    return -1;
}

void ept_invalidate(ept_context_t *ept)
{
    if (!ept) return;
    
    /* INVEPT with single-context invalidation */
    extern void vmx_invept(uint64_t type, uint64_t eptp);
    vmx_invept(1, ept->eptp);  /* Type 1 = single-context */
}
