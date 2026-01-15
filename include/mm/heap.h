/*
 * PureVisor - Kernel Heap Allocator Header
 * 
 * Slab allocator for kernel dynamic memory
 */

#ifndef _PUREVISOR_HEAP_H
#define _PUREVISOR_HEAP_H

#include <lib/types.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Slab sizes (power of 2) */
#define SLAB_MIN_SIZE       32
#define SLAB_MAX_SIZE       4096
#define SLAB_CACHE_COUNT    8       /* 32, 64, 128, 256, 512, 1024, 2048, 4096 */

/* Allocation flags */
#define GFP_KERNEL          0x00    /* Normal kernel allocation */
#define GFP_ZERO            0x01    /* Zero memory */
#define GFP_ATOMIC          0x02    /* Cannot sleep */
#define GFP_DMA             0x04    /* DMA-capable memory */

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/* Slab object header (embedded in free objects) */
typedef struct slab_obj {
    struct slab_obj *next;
} slab_obj_t;

/* Slab descriptor */
typedef struct slab {
    struct slab *next;          /* Next slab in list */
    struct slab *prev;          /* Prev slab in list */
    void *base;                 /* Base address of slab */
    slab_obj_t *free_list;      /* Free object list */
    uint32_t obj_size;          /* Object size */
    uint32_t obj_count;         /* Total objects */
    uint32_t free_count;        /* Free objects */
    uint32_t flags;             /* Slab flags */
} slab_t;

/* Slab cache */
typedef struct {
    const char *name;           /* Cache name */
    uint32_t obj_size;          /* Object size */
    uint32_t obj_per_slab;      /* Objects per slab */
    slab_t *slabs_partial;      /* Partially used slabs */
    slab_t *slabs_full;         /* Full slabs */
    slab_t *slabs_empty;        /* Empty slabs */
    uint64_t alloc_count;       /* Total allocations */
    uint64_t free_count;        /* Total frees */
} slab_cache_t;

/* Heap statistics */
typedef struct {
    uint64_t total_allocated;
    uint64_t total_freed;
    uint64_t current_usage;
    uint64_t peak_usage;
    uint64_t alloc_count;
    uint64_t free_count;
} heap_stats_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * heap_init - Initialize kernel heap
 */
void heap_init(void);

/**
 * kmalloc - Allocate kernel memory
 * @size: Size to allocate
 * @flags: Allocation flags (GFP_*)
 * 
 * Returns pointer to allocated memory or NULL
 */
void *kmalloc(size_t size, uint32_t flags);

/**
 * kfree - Free kernel memory
 * @ptr: Pointer to free
 */
void kfree(void *ptr);

/**
 * krealloc - Reallocate kernel memory
 * @ptr: Pointer to reallocate (NULL for new allocation)
 * @size: New size
 * @flags: Allocation flags
 * 
 * Returns pointer to reallocated memory or NULL
 */
void *krealloc(void *ptr, size_t size, uint32_t flags);

/**
 * kcalloc - Allocate and zero kernel memory
 * @count: Number of elements
 * @size: Size of each element
 * @flags: Allocation flags
 */
void *kcalloc(size_t count, size_t size, uint32_t flags);

/**
 * kstrdup - Duplicate string
 * @s: String to duplicate
 * @flags: Allocation flags
 */
char *kstrdup(const char *s, uint32_t flags);

/**
 * heap_get_stats - Get heap statistics
 */
void heap_get_stats(heap_stats_t *stats);

/**
 * heap_dump_stats - Print heap statistics
 */
void heap_dump_stats(void);

/* ============================================================================
 * Slab Cache API
 * ============================================================================ */

/**
 * slab_cache_create - Create a slab cache
 * @name: Cache name
 * @size: Object size
 * 
 * Returns cache pointer or NULL
 */
slab_cache_t *slab_cache_create(const char *name, size_t size);

/**
 * slab_cache_destroy - Destroy a slab cache
 * @cache: Cache to destroy
 */
void slab_cache_destroy(slab_cache_t *cache);

/**
 * slab_cache_alloc - Allocate from cache
 * @cache: Cache to allocate from
 * @flags: Allocation flags
 */
void *slab_cache_alloc(slab_cache_t *cache, uint32_t flags);

/**
 * slab_cache_free - Free to cache
 * @cache: Cache to free to
 * @ptr: Pointer to free
 */
void slab_cache_free(slab_cache_t *cache, void *ptr);

#endif /* _PUREVISOR_HEAP_H */
