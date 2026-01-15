/*
 * PureVisor - Physical Memory Manager Header
 * 
 * Buddy allocator for physical page allocation
 */

#ifndef _PUREVISOR_PMM_H
#define _PUREVISOR_PMM_H

#include <lib/types.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PMM_MAX_ORDER       11          /* Max block size: 2^11 * 4KB = 8MB */
#define PMM_MIN_ORDER       0           /* Min block size: 2^0 * 4KB = 4KB */

/* Memory zone types */
typedef enum {
    ZONE_DMA,           /* 0-16MB: DMA-capable memory */
    ZONE_NORMAL,        /* 16MB-4GB: Normal memory */
    ZONE_HIGH,          /* 4GB+: High memory */
    ZONE_COUNT
} mem_zone_t;

/* Page flags */
#define PAGE_FLAG_PRESENT       BIT(0)
#define PAGE_FLAG_FREE          BIT(1)
#define PAGE_FLAG_KERNEL        BIT(2)
#define PAGE_FLAG_USER          BIT(3)
#define PAGE_FLAG_RESERVED      BIT(4)
#define PAGE_FLAG_DMA           BIT(5)

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/* Physical page descriptor */
typedef struct page {
    uint32_t flags;             /* Page flags */
    uint32_t order;             /* Buddy order (0-11) */
    uint32_t refcount;          /* Reference count */
    uint32_t reserved;          /* Padding */
    struct page *next;          /* Next in free list */
    struct page *prev;          /* Prev in free list */
} page_t;

/* Free list for buddy allocator */
typedef struct {
    page_t *head;               /* First free block */
    uint64_t count;             /* Number of free blocks */
} free_list_t;

/* Memory zone descriptor */
typedef struct {
    phys_addr_t start;          /* Zone start address */
    phys_addr_t end;            /* Zone end address */
    uint64_t total_pages;       /* Total pages in zone */
    uint64_t free_pages;        /* Free pages in zone */
    free_list_t free_lists[PMM_MAX_ORDER + 1];  /* Free lists per order */
} zone_t;

/* PMM statistics */
typedef struct {
    uint64_t total_memory;      /* Total physical memory */
    uint64_t free_memory;       /* Free physical memory */
    uint64_t used_memory;       /* Used physical memory */
    uint64_t reserved_memory;   /* Reserved memory (BIOS, MMIO) */
    uint64_t kernel_memory;     /* Kernel memory usage */
    uint64_t page_count;        /* Total page descriptors */
    uint64_t alloc_count;       /* Allocation count */
    uint64_t free_count;        /* Free count */
} pmm_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * pmm_init - Initialize physical memory manager
 * @mmap: Memory map from bootloader
 * @mmap_size: Size of memory map
 * @entry_size: Size of each entry
 */
void pmm_init(void *mmap, uint32_t mmap_size, uint32_t entry_size);

/**
 * pmm_alloc_pages - Allocate contiguous physical pages
 * @order: Order of allocation (2^order pages)
 * 
 * Returns physical address or 0 on failure
 */
phys_addr_t pmm_alloc_pages(uint32_t order);

/**
 * pmm_alloc_page - Allocate a single physical page
 */
static inline phys_addr_t pmm_alloc_page(void)
{
    return pmm_alloc_pages(0);
}

/**
 * pmm_free_pages - Free physical pages
 * @addr: Physical address to free
 * @order: Order of allocation
 */
void pmm_free_pages(phys_addr_t addr, uint32_t order);

/**
 * pmm_free_page - Free a single physical page
 */
static inline void pmm_free_page(phys_addr_t addr)
{
    pmm_free_pages(addr, 0);
}

/**
 * pmm_get_page - Get page descriptor for physical address
 */
page_t *pmm_get_page(phys_addr_t addr);

/**
 * pmm_get_stats - Get PMM statistics
 */
void pmm_get_stats(pmm_stats_t *stats);

/**
 * pmm_dump_stats - Print PMM statistics
 */
void pmm_dump_stats(void);

/**
 * pmm_get_total_pages - Get total pages
 */
uint64_t pmm_get_total_pages(void);

/**
 * pmm_get_free_pages - Get free pages
 */
uint64_t pmm_get_free_pages(void);

/* ============================================================================
 * Physical/Virtual Address Conversion
 * ============================================================================ */

/* Higher half kernel offset */
#define KERNEL_OFFSET       0xFFFF800000000000ULL

/* Physical to virtual (kernel direct mapping) */
static inline void *phys_to_virt(phys_addr_t paddr)
{
    return (void *)(paddr + KERNEL_OFFSET);
}

/* Virtual to physical (kernel direct mapping) */
static inline phys_addr_t virt_to_phys(void *vaddr)
{
    return (phys_addr_t)vaddr - KERNEL_OFFSET;
}

/* Page frame number from physical address */
static inline uint64_t phys_to_pfn(phys_addr_t paddr)
{
    return paddr >> PAGE_SHIFT;
}

/* Physical address from page frame number */
static inline phys_addr_t pfn_to_phys(uint64_t pfn)
{
    return pfn << PAGE_SHIFT;
}

#endif /* _PUREVISOR_PMM_H */
