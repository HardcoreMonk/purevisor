/*
 * PureVisor - Physical Memory Manager Implementation
 * 
 * Buddy allocator for physical page allocation
 */

#include <lib/types.h>
#include <lib/string.h>
#include <mm/pmm.h>
#include <kernel/console.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static zone_t zones[ZONE_COUNT];
static page_t *page_array = NULL;
static uint64_t page_count = 0;
static pmm_stats_t pmm_stats;
static volatile uint32_t pmm_lock = 0;

extern char _kernel_end[];

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static inline void lock_acquire(void)
{
    while (__sync_lock_test_and_set(&pmm_lock, 1)) {
        __asm__ __volatile__("pause" ::: "memory");
    }
}

static inline void lock_release(void)
{
    __sync_lock_release(&pmm_lock);
}

static zone_t *get_zone(phys_addr_t addr)
{
    if (addr < 16 * MB) return &zones[ZONE_DMA];
    else if (addr < 4 * GB) return &zones[ZONE_NORMAL];
    else return &zones[ZONE_HIGH];
}

static void free_list_add(zone_t *zone, page_t *page, uint32_t order)
{
    free_list_t *list = &zone->free_lists[order];
    
    page->flags = PAGE_FLAG_FREE;
    page->order = order;
    page->next = list->head;
    page->prev = NULL;
    
    if (list->head) list->head->prev = page;
    list->head = page;
    list->count++;
    zone->free_pages += (1ULL << order);
}

static void free_list_remove(zone_t *zone, page_t *page, uint32_t order)
{
    free_list_t *list = &zone->free_lists[order];
    
    if (page->prev) page->prev->next = page->next;
    else list->head = page->next;
    if (page->next) page->next->prev = page->prev;
    
    page->next = NULL;
    page->prev = NULL;
    list->count--;
    zone->free_pages -= (1ULL << order);
}

static page_t *get_buddy(page_t *page, uint32_t order)
{
    uint64_t pfn = page - page_array;
    uint64_t buddy_pfn = pfn ^ (1ULL << order);
    if (buddy_pfn >= page_count) return NULL;
    return &page_array[buddy_pfn];
}

static void split_block(zone_t *zone, page_t *page, uint32_t cur, uint32_t target)
{
    while (cur > target) {
        cur--;
        page_t *buddy = page + (1ULL << cur);
        free_list_add(zone, buddy, cur);
    }
}

static void coalesce_buddies(zone_t *zone, page_t *page, uint32_t order)
{
    while (order < PMM_MAX_ORDER) {
        page_t *buddy = get_buddy(page, order);
        if (!buddy || !(buddy->flags & PAGE_FLAG_FREE) || buddy->order != order)
            break;
        free_list_remove(zone, buddy, order);
        if (buddy < page) page = buddy;
        order++;
    }
    free_list_add(zone, page, order);
}

/* ============================================================================
 * Multiboot2 Memory Map
 * ============================================================================ */

#define MULTIBOOT_MEMORY_AVAILABLE  1

typedef struct PACKED {
    uint64_t addr;
    uint64_t len;
    uint32_t type;
    uint32_t reserved;
} mmap_entry_t;

/* ============================================================================
 * Public API
 * ============================================================================ */

void pmm_init(void *mmap, uint32_t mmap_size, uint32_t entry_size)
{
    memset(&pmm_stats, 0, sizeof(pmm_stats));
    memset(zones, 0, sizeof(zones));
    
    zones[ZONE_DMA].start = 0;
    zones[ZONE_DMA].end = 16 * MB;
    zones[ZONE_NORMAL].start = 16 * MB;
    zones[ZONE_NORMAL].end = 4 * GB;
    zones[ZONE_HIGH].start = 4 * GB;
    zones[ZONE_HIGH].end = UINT64_MAX;
    
    pr_info("PMM: Initializing...");
    
    /* First pass: find highest address */
    phys_addr_t highest_addr = 0;
    uint8_t *entry_ptr = (uint8_t *)mmap;
    uint8_t *mmap_end = entry_ptr + mmap_size;
    
    while (entry_ptr < mmap_end) {
        mmap_entry_t *entry = (mmap_entry_t *)entry_ptr;
        phys_addr_t end = entry->addr + entry->len;
        if (end > highest_addr) highest_addr = end;
        pmm_stats.total_memory += entry->len;
        if (entry->type != MULTIBOOT_MEMORY_AVAILABLE)
            pmm_stats.reserved_memory += entry->len;
        entry_ptr += entry_size;
    }
    
    page_count = (highest_addr + PAGE_SIZE - 1) / PAGE_SIZE;
    pmm_stats.page_count = page_count;
    
    /* Page array placement */
    uint64_t page_array_size = ALIGN_UP(page_count * sizeof(page_t), PAGE_SIZE);
    phys_addr_t kernel_end = (phys_addr_t)_kernel_end;
    if (kernel_end >= KERNEL_OFFSET) kernel_end -= KERNEL_OFFSET;
    kernel_end = ALIGN_UP(kernel_end, PAGE_SIZE);
    
    phys_addr_t page_array_phys = kernel_end;
    page_array = (page_t *)phys_to_virt(page_array_phys);
    
    pr_info("PMM: Page array at 0x%llx, %llu KB", page_array_phys, page_array_size / KB);
    
    /* Initialize all pages as reserved */
    memset(page_array, 0, page_count * sizeof(page_t));
    for (uint64_t i = 0; i < page_count; i++)
        page_array[i].flags = PAGE_FLAG_RESERVED;
    
    phys_addr_t reserved_end = page_array_phys + page_array_size;
    pmm_stats.kernel_memory = reserved_end;
    
    /* Second pass: add available memory */
    entry_ptr = (uint8_t *)mmap;
    uint64_t available_pages = 0;
    
    while (entry_ptr < mmap_end) {
        mmap_entry_t *entry = (mmap_entry_t *)entry_ptr;
        
        if (entry->type == MULTIBOOT_MEMORY_AVAILABLE) {
            phys_addr_t start = ALIGN_UP(entry->addr, PAGE_SIZE);
            phys_addr_t end = ALIGN_DOWN(entry->addr + entry->len, PAGE_SIZE);
            if (start < reserved_end) start = reserved_end;
            
            while (start < end) {
                uint64_t pfn = start / PAGE_SIZE;
                if (pfn < page_count) {
                    page_t *page = &page_array[pfn];
                    zone_t *zone = get_zone(start);
                    free_list_add(zone, page, 0);
                    available_pages++;
                }
                start += PAGE_SIZE;
            }
        }
        entry_ptr += entry_size;
    }
    
    pmm_stats.free_memory = available_pages * PAGE_SIZE;
    pmm_stats.used_memory = pmm_stats.total_memory - pmm_stats.free_memory - 
                            pmm_stats.reserved_memory;
    
    pr_info("PMM: %llu pages available (%llu MB)", available_pages, 
            (available_pages * PAGE_SIZE) / MB);
}

phys_addr_t pmm_alloc_pages(uint32_t order)
{
    if (order > PMM_MAX_ORDER) return 0;
    
    lock_acquire();
    
    for (int z = ZONE_NORMAL; z >= ZONE_DMA; z--) {
        zone_t *zone = &zones[z];
        
        for (uint32_t o = order; o <= PMM_MAX_ORDER; o++) {
            if (zone->free_lists[o].head) {
                page_t *page = zone->free_lists[o].head;
                free_list_remove(zone, page, o);
                
                if (o > order) split_block(zone, page, o, order);
                
                page->flags = PAGE_FLAG_PRESENT | PAGE_FLAG_KERNEL;
                page->order = order;
                page->refcount = 1;
                
                pmm_stats.alloc_count++;
                pmm_stats.free_memory -= (1ULL << order) * PAGE_SIZE;
                pmm_stats.used_memory += (1ULL << order) * PAGE_SIZE;
                
                lock_release();
                return (page - page_array) * PAGE_SIZE;
            }
        }
    }
    
    lock_release();
    return 0;
}

void pmm_free_pages(phys_addr_t addr, uint32_t order)
{
    if (addr == 0 || order > PMM_MAX_ORDER) return;
    
    uint64_t pfn = addr / PAGE_SIZE;
    if (pfn >= page_count) return;
    
    lock_acquire();
    
    page_t *page = &page_array[pfn];
    if (page->flags & PAGE_FLAG_FREE) {
        lock_release();
        return;
    }
    
    zone_t *zone = get_zone(addr);
    pmm_stats.free_count++;
    pmm_stats.free_memory += (1ULL << order) * PAGE_SIZE;
    pmm_stats.used_memory -= (1ULL << order) * PAGE_SIZE;
    
    coalesce_buddies(zone, page, order);
    lock_release();
}

page_t *pmm_get_page(phys_addr_t addr)
{
    uint64_t pfn = addr / PAGE_SIZE;
    if (pfn >= page_count) return NULL;
    return &page_array[pfn];
}

void pmm_get_stats(pmm_stats_t *stats)
{
    lock_acquire();
    *stats = pmm_stats;
    lock_release();
}

void pmm_dump_stats(void)
{
    pmm_stats_t s;
    pmm_get_stats(&s);
    
    kprintf("\n=== PMM Statistics ===\n");
    kprintf("Total:    %llu MB\n", s.total_memory / MB);
    kprintf("Free:     %llu MB\n", s.free_memory / MB);
    kprintf("Used:     %llu MB\n", s.used_memory / MB);
    kprintf("Reserved: %llu MB\n", s.reserved_memory / MB);
    kprintf("Allocs:   %llu\n", s.alloc_count);
    kprintf("Frees:    %llu\n", s.free_count);
}

uint64_t pmm_get_total_pages(void)
{
    uint64_t total = 0;
    for (int i = 0; i < ZONE_COUNT; i++) {
        total += zones[i].total_pages;
    }
    return total;
}

uint64_t pmm_get_free_pages(void)
{
    uint64_t free = 0;
    for (int i = 0; i < ZONE_COUNT; i++) {
        free += zones[i].free_pages;
    }
    return free;
}
