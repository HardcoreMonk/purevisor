/*
 * PureVisor - Virtual Memory Manager Header
 * 
 * x86_64 4-level paging management
 */

#ifndef _PUREVISOR_VMM_PAGING_H
#define _PUREVISOR_VMM_PAGING_H

#include <lib/types.h>

/* ============================================================================
 * Page Table Entry Flags
 * ============================================================================ */

#define PTE_PRESENT         BIT(0)      /* Page is present */
#define PTE_WRITABLE        BIT(1)      /* Page is writable */
#define PTE_USER            BIT(2)      /* User-mode accessible */
#define PTE_WRITE_THROUGH   BIT(3)      /* Write-through caching */
#define PTE_CACHE_DISABLE   BIT(4)      /* Cache disabled */
#define PTE_ACCESSED        BIT(5)      /* Page was accessed */
#define PTE_DIRTY           BIT(6)      /* Page was written */
#define PTE_HUGE            BIT(7)      /* Huge page (2MB/1GB) */
#define PTE_GLOBAL          BIT(8)      /* Global page */
#define PTE_NO_EXECUTE      BIT(63)     /* No execute (NX) */

/* Address mask for 4KB pages */
#define PTE_ADDR_MASK       0x000FFFFFFFFFF000ULL

/* Page table indices */
#define PML4_INDEX(addr)    (((addr) >> 39) & 0x1FF)
#define PDPT_INDEX(addr)    (((addr) >> 30) & 0x1FF)
#define PD_INDEX(addr)      (((addr) >> 21) & 0x1FF)
#define PT_INDEX(addr)      (((addr) >> 12) & 0x1FF)
#define PAGE_OFFSET(addr)   ((addr) & 0xFFF)

/* Number of entries per table */
#define PT_ENTRIES          512

/* ============================================================================
 * Page Table Types
 * ============================================================================ */

typedef uint64_t pte_t;     /* Page table entry */
typedef pte_t *pt_t;        /* Page table */
typedef pte_t *pd_t;        /* Page directory */
typedef pte_t *pdpt_t;      /* Page directory pointer table */
typedef pte_t *pml4_t;      /* Page map level 4 */

/* ============================================================================
 * Virtual Memory Context
 * ============================================================================ */

typedef struct {
    pml4_t pml4;            /* Top-level page table (physical) */
    phys_addr_t pml4_phys;  /* Physical address of PML4 */
    uint64_t flags;         /* Context flags */
} vm_context_t;

/* ============================================================================
 * Mapping Flags
 * ============================================================================ */

#define MAP_KERNEL          0x00    /* Kernel mapping */
#define MAP_USER            0x01    /* User mapping */
#define MAP_WRITE           0x02    /* Writable */
#define MAP_EXEC            0x04    /* Executable */
#define MAP_NOCACHE         0x08    /* Non-cacheable */
#define MAP_HUGE_2M         0x10    /* Use 2MB pages */
#define MAP_HUGE_1G         0x20    /* Use 1GB pages */

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * paging_init - Initialize paging subsystem
 * 
 * Sets up kernel page tables
 */
void paging_init(void);

/**
 * paging_create_context - Create a new VM context
 * 
 * Returns new context or NULL on failure
 */
vm_context_t *paging_create_context(void);

/**
 * paging_destroy_context - Destroy a VM context
 * @ctx: Context to destroy
 */
void paging_destroy_context(vm_context_t *ctx);

/**
 * paging_switch_context - Switch to a VM context
 * @ctx: Context to switch to (NULL for kernel)
 */
void paging_switch_context(vm_context_t *ctx);

/**
 * paging_map - Map virtual to physical address
 * @ctx: VM context (NULL for kernel)
 * @virt: Virtual address
 * @phys: Physical address
 * @size: Size to map
 * @flags: Mapping flags
 * 
 * Returns 0 on success, -1 on failure
 */
int paging_map(vm_context_t *ctx, virt_addr_t virt, phys_addr_t phys,
               size_t size, uint32_t flags);

/**
 * paging_unmap - Unmap virtual address range
 * @ctx: VM context (NULL for kernel)
 * @virt: Virtual address
 * @size: Size to unmap
 * 
 * Returns 0 on success, -1 on failure
 */
int paging_unmap(vm_context_t *ctx, virt_addr_t virt, size_t size);

/**
 * paging_get_phys - Get physical address for virtual address
 * @ctx: VM context (NULL for kernel)
 * @virt: Virtual address
 * 
 * Returns physical address or 0 if not mapped
 */
phys_addr_t paging_get_phys(vm_context_t *ctx, virt_addr_t virt);

/**
 * paging_set_flags - Modify page flags
 * @ctx: VM context
 * @virt: Virtual address
 * @size: Size
 * @flags: New flags
 */
int paging_set_flags(vm_context_t *ctx, virt_addr_t virt, size_t size, 
                     uint32_t flags);

/**
 * paging_flush_tlb - Flush TLB for address
 * @virt: Virtual address to flush
 */
static inline void paging_flush_tlb(virt_addr_t virt)
{
    __asm__ __volatile__("invlpg (%0)" :: "r"(virt) : "memory");
}

/**
 * paging_flush_tlb_all - Flush entire TLB
 */
static inline void paging_flush_tlb_all(void)
{
    uint64_t cr3;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(cr3));
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(cr3));
}

/**
 * paging_get_kernel_context - Get kernel VM context
 */
vm_context_t *paging_get_kernel_context(void);

#endif /* _PUREVISOR_VMM_PAGING_H */
