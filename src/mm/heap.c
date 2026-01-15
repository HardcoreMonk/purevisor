/*
 * PureVisor - Kernel Heap Allocator Implementation
 * 
 * Simple allocator for kernel dynamic memory
 */

#include <lib/types.h>
#include <lib/string.h>
#include <mm/heap.h>
#include <mm/pmm.h>
#include <kernel/console.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define HEAP_MAGIC          0xDEADBEEF
#define HEAP_INITIAL_PAGES  16

typedef struct block_header {
    uint32_t magic;
    uint32_t size;
    uint32_t flags;
    uint32_t reserved;
    struct block_header *next;
    struct block_header *prev;
} block_header_t;

#define BLOCK_FLAG_FREE     BIT(0)
#define HEADER_SIZE         ALIGN_UP(sizeof(block_header_t), 16)
#define MIN_BLOCK_SIZE      (HEADER_SIZE + 16)

/* ============================================================================
 * Global State
 * ============================================================================ */

static block_header_t *free_list = NULL;
static void *heap_start = NULL;
static void *heap_end = NULL;
static volatile uint32_t heap_lock = 0;
static heap_stats_t heap_stats;
static bool heap_initialized = false;

/* ============================================================================
 * Lock Functions
 * ============================================================================ */

static inline void lock_acquire(void)
{
    while (__sync_lock_test_and_set(&heap_lock, 1)) {
        __asm__ __volatile__("pause" ::: "memory");
    }
}

static inline void lock_release(void)
{
    __sync_lock_release(&heap_lock);
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static void free_list_add(block_header_t *block)
{
    block->flags |= BLOCK_FLAG_FREE;
    
    if (!free_list || block < free_list) {
        block->next = free_list;
        block->prev = NULL;
        if (free_list) free_list->prev = block;
        free_list = block;
        return;
    }
    
    block_header_t *curr = free_list;
    while (curr->next && curr->next < block) {
        curr = curr->next;
    }
    
    block->next = curr->next;
    block->prev = curr;
    if (curr->next) curr->next->prev = block;
    curr->next = block;
}

static void free_list_remove(block_header_t *block)
{
    if (block->prev) block->prev->next = block->next;
    else free_list = block->next;
    
    if (block->next) block->next->prev = block->prev;
    
    block->next = NULL;
    block->prev = NULL;
    block->flags &= ~BLOCK_FLAG_FREE;
}

static void coalesce(block_header_t *block)
{
    /* Coalesce with next block */
    if (block->next) {
        void *block_end = (void *)block + block->size;
        if (block_end == block->next && (block->next->flags & BLOCK_FLAG_FREE)) {
            block_header_t *next = block->next;
            block->size += next->size;
            block->next = next->next;
            if (next->next) next->next->prev = block;
        }
    }
    
    /* Coalesce with previous block */
    if (block->prev && (block->prev->flags & BLOCK_FLAG_FREE)) {
        void *prev_end = (void *)block->prev + block->prev->size;
        if (prev_end == block) {
            block_header_t *prev = block->prev;
            prev->size += block->size;
            prev->next = block->next;
            if (block->next) block->next->prev = prev;
        }
    }
}

static block_header_t *find_free_block(size_t size)
{
    block_header_t *curr = free_list;
    block_header_t *best = NULL;
    
    /* Best-fit search */
    while (curr) {
        if ((curr->flags & BLOCK_FLAG_FREE) && curr->size >= size) {
            if (!best || curr->size < best->size) {
                best = curr;
                if (curr->size == size) break;
            }
        }
        curr = curr->next;
    }
    
    return best;
}

static void split_block(block_header_t *block, size_t size)
{
    size_t remaining = block->size - size;
    
    if (remaining >= MIN_BLOCK_SIZE) {
        block_header_t *new_block = (block_header_t *)((void *)block + size);
        new_block->magic = HEAP_MAGIC;
        new_block->size = remaining;
        new_block->flags = 0;
        new_block->next = block->next;
        new_block->prev = block;
        
        if (block->next) block->next->prev = new_block;
        block->next = new_block;
        block->size = size;
        
        free_list_add(new_block);
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void heap_init(void)
{
    pr_info("Heap: Initializing kernel heap...");
    
    memset(&heap_stats, 0, sizeof(heap_stats));
    
    /* Allocate initial heap pages */
    phys_addr_t heap_phys = pmm_alloc_pages(4);  /* 16 pages = 64KB */
    if (!heap_phys) {
        pr_error("Heap: Failed to allocate initial heap");
        return;
    }
    
    heap_start = phys_to_virt(heap_phys);
    heap_end = heap_start + (16 * PAGE_SIZE);
    
    /* Initialize first free block */
    block_header_t *first = (block_header_t *)heap_start;
    first->magic = HEAP_MAGIC;
    first->size = (16 * PAGE_SIZE);
    first->flags = BLOCK_FLAG_FREE;
    first->next = NULL;
    first->prev = NULL;
    
    free_list = first;
    heap_initialized = true;
    
    pr_info("Heap: %llu KB available at 0x%p", (16 * PAGE_SIZE) / KB, heap_start);
}

void *kmalloc(size_t size, uint32_t flags)
{
    if (!heap_initialized || size == 0) return NULL;
    
    /* Align size and add header */
    size = ALIGN_UP(size, 16) + HEADER_SIZE;
    if (size < MIN_BLOCK_SIZE) size = MIN_BLOCK_SIZE;
    
    lock_acquire();
    
    block_header_t *block = find_free_block(size);
    
    if (!block) {
        /* Need to expand heap */
        uint32_t pages_needed = (size + PAGE_SIZE - 1) / PAGE_SIZE;
        if (pages_needed < 4) pages_needed = 4;
        
        phys_addr_t new_phys = pmm_alloc_pages(pages_needed > 1 ? 2 : 0);
        if (!new_phys) {
            lock_release();
            return NULL;
        }
        
        void *new_mem = phys_to_virt(new_phys);
        block = (block_header_t *)new_mem;
        block->magic = HEAP_MAGIC;
        block->size = (1 << (pages_needed > 1 ? 2 : 0)) * PAGE_SIZE;
        block->flags = 0;
        block->next = NULL;
        block->prev = NULL;
        
        free_list_add(block);
        
        if (new_mem + block->size > heap_end) {
            heap_end = new_mem + block->size;
        }
    }
    
    free_list_remove(block);
    split_block(block, size);
    
    block->flags &= ~BLOCK_FLAG_FREE;
    
    heap_stats.alloc_count++;
    heap_stats.total_allocated += block->size;
    heap_stats.current_usage += block->size;
    if (heap_stats.current_usage > heap_stats.peak_usage) {
        heap_stats.peak_usage = heap_stats.current_usage;
    }
    
    lock_release();
    
    void *ptr = (void *)block + HEADER_SIZE;
    
    if (flags & GFP_ZERO) {
        memset(ptr, 0, size - HEADER_SIZE);
    }
    
    return ptr;
}

void kfree(void *ptr)
{
    if (!ptr || !heap_initialized) return;
    
    block_header_t *block = (block_header_t *)(ptr - HEADER_SIZE);
    
    if (block->magic != HEAP_MAGIC) {
        pr_error("Heap: Invalid free at 0x%p", ptr);
        return;
    }
    
    if (block->flags & BLOCK_FLAG_FREE) {
        pr_error("Heap: Double free at 0x%p", ptr);
        return;
    }
    
    lock_acquire();
    
    heap_stats.free_count++;
    heap_stats.total_freed += block->size;
    heap_stats.current_usage -= block->size;
    
    free_list_add(block);
    coalesce(block);
    
    lock_release();
}

void *krealloc(void *ptr, size_t size, uint32_t flags)
{
    if (!ptr) return kmalloc(size, flags);
    if (size == 0) {
        kfree(ptr);
        return NULL;
    }
    
    block_header_t *block = (block_header_t *)(ptr - HEADER_SIZE);
    size_t old_size = block->size - HEADER_SIZE;
    
    if (size <= old_size) return ptr;
    
    void *new_ptr = kmalloc(size, flags);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, old_size);
    kfree(ptr);
    
    return new_ptr;
}

void *kcalloc(size_t count, size_t size, uint32_t flags)
{
    size_t total = count * size;
    return kmalloc(total, flags | GFP_ZERO);
}

char *kstrdup(const char *s, uint32_t flags)
{
    if (!s) return NULL;
    
    size_t len = strlen(s) + 1;
    char *dup = kmalloc(len, flags);
    if (dup) memcpy(dup, s, len);
    
    return dup;
}

void heap_get_stats(heap_stats_t *stats)
{
    lock_acquire();
    *stats = heap_stats;
    lock_release();
}

void heap_dump_stats(void)
{
    heap_stats_t s;
    heap_get_stats(&s);
    
    kprintf("\n=== Heap Statistics ===\n");
    kprintf("Allocated: %llu KB\n", s.total_allocated / KB);
    kprintf("Freed:     %llu KB\n", s.total_freed / KB);
    kprintf("Current:   %llu KB\n", s.current_usage / KB);
    kprintf("Peak:      %llu KB\n", s.peak_usage / KB);
    kprintf("Allocs:    %llu\n", s.alloc_count);
    kprintf("Frees:     %llu\n", s.free_count);
}

/* ============================================================================
 * Slab Cache (Simplified)
 * ============================================================================ */

slab_cache_t *slab_cache_create(const char *name, size_t size)
{
    slab_cache_t *cache = kmalloc(sizeof(slab_cache_t), GFP_KERNEL | GFP_ZERO);
    if (!cache) return NULL;
    
    cache->name = name;
    cache->obj_size = ALIGN_UP(size, 8);
    cache->obj_per_slab = (PAGE_SIZE - sizeof(slab_t)) / cache->obj_size;
    
    return cache;
}

void slab_cache_destroy(slab_cache_t *cache)
{
    if (!cache) return;
    
    /* Free all allocated slabs
     * Note: In the current simplified implementation, individual objects
     * are allocated via kmalloc and tracked per-allocation. Full slab
     * freeing would require tracking all allocated pages for this cache.
     * 
     * For production use, maintain a list of slab pages:
     * - Walk through slab->next linked list
     * - Free each slab page back to PMM
     */
    
    /* Reset statistics */
    cache->alloc_count = 0;
    cache->free_count = 0;
    
    /* Free the cache descriptor itself */
    kfree(cache);
}

void *slab_cache_alloc(slab_cache_t *cache, uint32_t flags)
{
    if (!cache) return NULL;
    
    /* Simplified: just use kmalloc */
    void *ptr = kmalloc(cache->obj_size, flags);
    if (ptr) cache->alloc_count++;
    
    return ptr;
}

void slab_cache_free(slab_cache_t *cache, void *ptr)
{
    if (!cache || !ptr) return;
    
    cache->free_count++;
    kfree(ptr);
}
