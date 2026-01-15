/*
 * PureVisor - Storage Pool Header
 * 
 * Pooled storage management with replication
 */

#ifndef _PUREVISOR_STORAGE_POOL_H
#define _PUREVISOR_STORAGE_POOL_H

#include <lib/types.h>
#include <storage/block.h>

/* ============================================================================
 * Pool Constants
 * ============================================================================ */

#define POOL_MAX_NAME           64
#define POOL_MAX_DEVICES        16
#define POOL_MAX_VOLUMES        64

#define POOL_EXTENT_SIZE        (4 * MB)    /* 4MB extents */
#define POOL_MAX_EXTENTS        65536       /* 256GB max per pool */

/* Pool states */
#define POOL_STATE_OFFLINE      0
#define POOL_STATE_DEGRADED     1
#define POOL_STATE_ONLINE       2
#define POOL_STATE_REBUILDING   3

/* Replication modes */
#define POOL_REPL_NONE          0   /* No replication */
#define POOL_REPL_MIRROR        1   /* Mirroring (2 copies) */
#define POOL_REPL_TRIPLE        2   /* Triple mirroring */
#define POOL_REPL_ERASURE       3   /* Erasure coding */

/* ============================================================================
 * Extent Management
 * ============================================================================ */

#define EXTENT_FREE             0
#define EXTENT_ALLOCATED        1
#define EXTENT_RESERVED         2

typedef struct extent_info {
    uint32_t state;
    uint32_t volume_id;         /* Owning volume */
    uint64_t volume_offset;     /* Offset within volume */
    uint32_t device_id;         /* Physical device */
    uint64_t device_offset;     /* Offset on device */
    uint32_t replica_count;     /* Number of replicas */
    uint32_t replica_extents[3];/* Replica extent IDs */
} extent_info_t;

/* ============================================================================
 * Volume
 * ============================================================================ */

typedef struct storage_volume {
    char name[POOL_MAX_NAME];
    char uuid[BLOCK_MAX_UUID];
    uint32_t id;
    
    /* Size */
    uint64_t size;              /* Logical size */
    uint64_t allocated;         /* Allocated extents */
    
    /* Properties */
    uint32_t replication;       /* Replication mode */
    bool thin_provisioned;
    bool online;
    
    /* Extent map */
    uint32_t *extent_map;       /* extent_map[vol_extent] = pool_extent */
    uint32_t num_extents;
    
    /* Parent pool */
    struct storage_pool *pool;
    
    /* Block device interface */
    block_device_t blkdev;
    
    /* List */
    struct storage_volume *next;
} storage_volume_t;

/* ============================================================================
 * Storage Pool
 * ============================================================================ */

typedef struct storage_pool {
    char name[POOL_MAX_NAME];
    char uuid[BLOCK_MAX_UUID];
    uint32_t id;
    
    /* State */
    uint32_t state;
    
    /* Capacity */
    uint64_t total_size;
    uint64_t free_size;
    uint64_t used_size;
    
    /* Physical devices */
    block_device_t *devices[POOL_MAX_DEVICES];
    uint32_t device_count;
    
    /* Extent management */
    extent_info_t *extents;
    uint32_t total_extents;
    uint32_t free_extents;
    uint32_t next_extent;
    
    /* Volumes */
    storage_volume_t *volumes;
    uint32_t volume_count;
    
    /* Default settings */
    uint32_t default_replication;
    bool default_thin;
    
    /* Statistics */
    uint64_t read_ops;
    uint64_t write_ops;
    uint64_t read_bytes;
    uint64_t write_bytes;
    
    /* List */
    struct storage_pool *next;
} storage_pool_t;

/* ============================================================================
 * Pool API
 * ============================================================================ */

/**
 * pool_create - Create a new storage pool
 * @name: Pool name
 */
storage_pool_t *pool_create(const char *name);

/**
 * pool_destroy - Destroy a storage pool
 */
void pool_destroy(storage_pool_t *pool);

/**
 * pool_add_device - Add a device to pool
 * @pool: Target pool
 * @dev: Block device to add
 */
int pool_add_device(storage_pool_t *pool, block_device_t *dev);

/**
 * pool_remove_device - Remove device from pool
 * @pool: Target pool
 * @dev: Device to remove
 */
int pool_remove_device(storage_pool_t *pool, block_device_t *dev);

/**
 * pool_get_status - Get pool status
 */
int pool_get_status(storage_pool_t *pool);

/* ============================================================================
 * Volume API
 * ============================================================================ */

/**
 * volume_create - Create a volume in pool
 * @pool: Parent pool
 * @name: Volume name
 * @size: Size in bytes
 * @replication: Replication mode
 * @thin: Thin provisioning
 */
storage_volume_t *volume_create(storage_pool_t *pool, const char *name,
                                 uint64_t size, uint32_t replication, bool thin);

/**
 * volume_destroy - Destroy a volume
 */
void volume_destroy(storage_volume_t *vol);

/**
 * volume_resize - Resize a volume
 */
int volume_resize(storage_volume_t *vol, uint64_t new_size);

/**
 * volume_snapshot - Create a snapshot
 */
storage_volume_t *volume_snapshot(storage_volume_t *vol, const char *name);

/**
 * volume_get_block_device - Get block device for volume
 */
block_device_t *volume_get_block_device(storage_volume_t *vol);

/* ============================================================================
 * Extent API
 * ============================================================================ */

/**
 * pool_alloc_extent - Allocate an extent
 */
int pool_alloc_extent(storage_pool_t *pool, uint32_t *extent_id);

/**
 * pool_free_extent - Free an extent
 */
void pool_free_extent(storage_pool_t *pool, uint32_t extent_id);

/**
 * pool_alloc_replicated_extent - Allocate extent with replicas
 */
int pool_alloc_replicated_extent(storage_pool_t *pool, uint32_t replication,
                                  uint32_t *extent_ids);

#endif /* _PUREVISOR_STORAGE_POOL_H */
