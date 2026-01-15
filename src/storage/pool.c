/*
 * PureVisor - Storage Pool Implementation
 * 
 * Pooled storage with extent-based allocation and replication
 */

#include <lib/types.h>
#include <lib/string.h>
#include <storage/pool.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static storage_pool_t *pools = NULL;
static uint32_t pool_count = 0;
static uint32_t next_pool_id = 1;
static uint32_t next_volume_id = 1;

/* ============================================================================
 * Volume Block Operations
 * ============================================================================ */

static int volume_submit(block_device_t *dev, block_request_t *req)
{
    storage_volume_t *vol = (storage_volume_t *)dev->priv;
    storage_pool_t *pool = vol->pool;
    
    if (!vol->online || pool->state == POOL_STATE_OFFLINE) {
        req->status = -1;
        if (req->completion) req->completion(req->completion_ctx, -1);
        return -1;
    }
    
    /* Calculate extent */
    uint32_t extent_idx = req->offset / POOL_EXTENT_SIZE;
    uint64_t extent_offset = req->offset % POOL_EXTENT_SIZE;
    
    if (extent_idx >= vol->num_extents) {
        req->status = -1;
        if (req->completion) req->completion(req->completion_ctx, -1);
        return -1;
    }
    
    uint32_t pool_extent = vol->extent_map[extent_idx];
    
    /* Handle thin provisioning - allocate on write */
    if (pool_extent == 0 && req->op == BLOCK_OP_WRITE) {
        if (pool_alloc_extent(pool, &pool_extent) != 0) {
            req->status = -1;
            if (req->completion) req->completion(req->completion_ctx, -1);
            return -1;
        }
        vol->extent_map[extent_idx] = pool_extent;
        vol->allocated += POOL_EXTENT_SIZE;
    }
    
    /* Unallocated read returns zeros */
    if (pool_extent == 0 && req->op == BLOCK_OP_READ) {
        memset(req->buffer, 0, req->length);
        req->status = 0;
        if (req->completion) req->completion(req->completion_ctx, 0);
        return 0;
    }
    
    /* Get physical location */
    extent_info_t *ext = &pool->extents[pool_extent];
    block_device_t *phys_dev = pool->devices[ext->device_id];
    uint64_t phys_offset = ext->device_offset + extent_offset;
    
    /* Perform I/O */
    int ret = 0;
    if (req->op == BLOCK_OP_READ) {
        ret = block_read(phys_dev, phys_offset, req->buffer, req->length);
        pool->read_ops++;
        pool->read_bytes += req->length;
    } else if (req->op == BLOCK_OP_WRITE) {
        ret = block_write(phys_dev, phys_offset, req->buffer, req->length);
        
        /* Write to replicas */
        for (uint32_t r = 0; r < ext->replica_count; r++) {
            uint32_t rep_ext = ext->replica_extents[r];
            extent_info_t *rep = &pool->extents[rep_ext];
            block_device_t *rep_dev = pool->devices[rep->device_id];
            uint64_t rep_offset = rep->device_offset + extent_offset;
            block_write(rep_dev, rep_offset, req->buffer, req->length);
        }
        
        pool->write_ops++;
        pool->write_bytes += req->length;
    }
    
    req->status = ret;
    if (req->completion) req->completion(req->completion_ctx, ret);
    
    return ret;
}

static int volume_flush(block_device_t *dev)
{
    storage_volume_t *vol = (storage_volume_t *)dev->priv;
    storage_pool_t *pool = vol->pool;
    
    /* Flush all devices in pool */
    for (uint32_t i = 0; i < pool->device_count; i++) {
        block_flush(pool->devices[i]);
    }
    
    return 0;
}

static const block_ops_t volume_ops = {
    .submit = volume_submit,
    .flush = volume_flush,
};

/* ============================================================================
 * Extent Management
 * ============================================================================ */

int pool_alloc_extent(storage_pool_t *pool, uint32_t *extent_id)
{
    if (pool->free_extents == 0) {
        return -1;
    }
    
    /* Find free extent */
    for (uint32_t i = pool->next_extent; i < pool->total_extents; i++) {
        if (pool->extents[i].state == EXTENT_FREE) {
            pool->extents[i].state = EXTENT_ALLOCATED;
            pool->free_extents--;
            pool->next_extent = i + 1;
            *extent_id = i;
            return 0;
        }
    }
    
    /* Wrap around */
    for (uint32_t i = 1; i < pool->next_extent; i++) {
        if (pool->extents[i].state == EXTENT_FREE) {
            pool->extents[i].state = EXTENT_ALLOCATED;
            pool->free_extents--;
            pool->next_extent = i + 1;
            *extent_id = i;
            return 0;
        }
    }
    
    return -1;
}

void pool_free_extent(storage_pool_t *pool, uint32_t extent_id)
{
    if (extent_id > 0 && extent_id < pool->total_extents) {
        pool->extents[extent_id].state = EXTENT_FREE;
        pool->extents[extent_id].volume_id = 0;
        pool->free_extents++;
    }
}

int pool_alloc_replicated_extent(storage_pool_t *pool, uint32_t replication,
                                  uint32_t *extent_ids)
{
    uint32_t needed = replication + 1;  /* Primary + replicas */
    
    if (pool->free_extents < needed) {
        return -1;
    }
    
    /* Allocate primary */
    if (pool_alloc_extent(pool, &extent_ids[0]) != 0) {
        return -1;
    }
    
    /* Allocate replicas on different devices if possible */
    for (uint32_t r = 1; r <= replication; r++) {
        if (pool_alloc_extent(pool, &extent_ids[r]) != 0) {
            /* Rollback */
            for (uint32_t i = 0; i < r; i++) {
                pool_free_extent(pool, extent_ids[i]);
            }
            return -1;
        }
        
        /* Link replica to primary */
        pool->extents[extent_ids[0]].replica_extents[r-1] = extent_ids[r];
    }
    
    pool->extents[extent_ids[0]].replica_count = replication;
    
    return 0;
}

/* ============================================================================
 * Pool Management
 * ============================================================================ */

storage_pool_t *pool_create(const char *name)
{
    storage_pool_t *pool = kmalloc(sizeof(storage_pool_t), GFP_KERNEL | GFP_ZERO);
    if (!pool) return NULL;
    
    strncpy(pool->name, name, POOL_MAX_NAME - 1);
    block_generate_uuid(pool->uuid);
    pool->id = next_pool_id++;
    
    pool->state = POOL_STATE_OFFLINE;
    pool->default_replication = POOL_REPL_NONE;
    pool->default_thin = true;
    
    /* Add to list */
    pool->next = pools;
    pools = pool;
    pool_count++;
    
    pr_info("Pool: Created '%s' (%s)", pool->name, pool->uuid);
    
    return pool;
}

void pool_destroy(storage_pool_t *pool)
{
    if (!pool) return;
    
    /* Destroy all volumes */
    while (pool->volumes) {
        volume_destroy(pool->volumes);
    }
    
    /* Free extents */
    if (pool->extents) {
        kfree(pool->extents);
    }
    
    /* Remove from list */
    storage_pool_t **pp = &pools;
    while (*pp) {
        if (*pp == pool) {
            *pp = pool->next;
            pool_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    pr_info("Pool: Destroyed '%s'", pool->name);
    kfree(pool);
}

int pool_add_device(storage_pool_t *pool, block_device_t *dev)
{
    if (!pool || !dev) return -1;
    if (pool->device_count >= POOL_MAX_DEVICES) return -1;
    
    uint32_t dev_idx = pool->device_count;
    pool->devices[dev_idx] = dev;
    pool->device_count++;
    
    /* Calculate extents from this device */
    uint64_t dev_extents = dev->size / POOL_EXTENT_SIZE;
    uint32_t old_total = pool->total_extents;
    uint32_t new_total = old_total + dev_extents;
    
    /* Reallocate extent array */
    extent_info_t *new_extents = kmalloc(new_total * sizeof(extent_info_t), 
                                          GFP_KERNEL | GFP_ZERO);
    if (!new_extents) {
        pool->device_count--;
        return -1;
    }
    
    if (pool->extents) {
        memcpy(new_extents, pool->extents, old_total * sizeof(extent_info_t));
        kfree(pool->extents);
    }
    pool->extents = new_extents;
    
    /* Initialize new extents */
    uint64_t offset = 0;
    for (uint32_t i = old_total; i < new_total; i++) {
        pool->extents[i].state = EXTENT_FREE;
        pool->extents[i].device_id = dev_idx;
        pool->extents[i].device_offset = offset;
        offset += POOL_EXTENT_SIZE;
    }
    
    pool->total_extents = new_total;
    pool->free_extents += dev_extents;
    pool->total_size += dev_extents * POOL_EXTENT_SIZE;
    pool->free_size = pool->free_extents * POOL_EXTENT_SIZE;
    
    if (pool->state == POOL_STATE_OFFLINE) {
        pool->state = POOL_STATE_ONLINE;
    }
    
    pr_info("Pool: Added device '%s' to '%s' (+%llu MB)",
            dev->name, pool->name, dev_extents * POOL_EXTENT_SIZE / MB);
    
    return 0;
}

int pool_remove_device(storage_pool_t *pool, block_device_t *dev)
{
    /* Find device */
    int dev_idx = -1;
    for (uint32_t i = 0; i < pool->device_count; i++) {
        if (pool->devices[i] == dev) {
            dev_idx = i;
            break;
        }
    }
    
    if (dev_idx < 0) return -1;
    
    /* Check if any extents are in use */
    for (uint32_t i = 0; i < pool->total_extents; i++) {
        if (pool->extents[i].device_id == (uint32_t)dev_idx &&
            pool->extents[i].state == EXTENT_ALLOCATED) {
            pr_error("Pool: Cannot remove device with allocated extents");
            return -1;
        }
    }
    
    /* Remove device */
    for (uint32_t i = dev_idx; i < pool->device_count - 1; i++) {
        pool->devices[i] = pool->devices[i + 1];
    }
    pool->device_count--;
    
    pr_info("Pool: Removed device from '%s'", pool->name);
    return 0;
}

int pool_get_status(storage_pool_t *pool)
{
    if (!pool) return POOL_STATE_OFFLINE;
    return pool->state;
}

/* ============================================================================
 * Volume Management
 * ============================================================================ */

storage_volume_t *volume_create(storage_pool_t *pool, const char *name,
                                 uint64_t size, uint32_t replication, bool thin)
{
    if (!pool || pool->state == POOL_STATE_OFFLINE) {
        return NULL;
    }
    
    /* Calculate required extents */
    uint32_t num_extents = (size + POOL_EXTENT_SIZE - 1) / POOL_EXTENT_SIZE;
    uint32_t needed = num_extents * (replication + 1);
    
    if (!thin && pool->free_extents < needed) {
        pr_error("Pool: Not enough space for volume");
        return NULL;
    }
    
    storage_volume_t *vol = kmalloc(sizeof(storage_volume_t), GFP_KERNEL | GFP_ZERO);
    if (!vol) return NULL;
    
    strncpy(vol->name, name, POOL_MAX_NAME - 1);
    block_generate_uuid(vol->uuid);
    vol->id = next_volume_id++;
    
    vol->size = num_extents * POOL_EXTENT_SIZE;
    vol->replication = replication;
    vol->thin_provisioned = thin;
    vol->pool = pool;
    vol->num_extents = num_extents;
    
    /* Allocate extent map */
    vol->extent_map = kmalloc(num_extents * sizeof(uint32_t), GFP_KERNEL | GFP_ZERO);
    if (!vol->extent_map) {
        kfree(vol);
        return NULL;
    }
    
    /* Pre-allocate if not thin */
    if (!thin) {
        for (uint32_t i = 0; i < num_extents; i++) {
            uint32_t ext_ids[4];
            if (pool_alloc_replicated_extent(pool, replication, ext_ids) != 0) {
                /* Rollback */
                for (uint32_t j = 0; j < i; j++) {
                    pool_free_extent(pool, vol->extent_map[j]);
                }
                kfree(vol->extent_map);
                kfree(vol);
                return NULL;
            }
            vol->extent_map[i] = ext_ids[0];
            pool->extents[ext_ids[0]].volume_id = vol->id;
            pool->extents[ext_ids[0]].volume_offset = i * POOL_EXTENT_SIZE;
        }
        vol->allocated = vol->size;
        pool->used_size += vol->size;
        pool->free_size -= vol->size;
    }
    
    /* Setup block device interface */
    strncpy(vol->blkdev.name, name, BLOCK_MAX_NAME - 1);
    strcpy(vol->blkdev.uuid, vol->uuid);
    vol->blkdev.size = vol->size;
    vol->blkdev.block_size = BLOCK_DEFAULT_SIZE;
    vol->blkdev.num_blocks = vol->size / BLOCK_DEFAULT_SIZE;
    vol->blkdev.ops = &volume_ops;
    vol->blkdev.priv = vol;
    
    vol->online = true;
    
    /* Add to pool */
    vol->next = pool->volumes;
    pool->volumes = vol;
    pool->volume_count++;
    
    /* Register block device */
    block_register(&vol->blkdev);
    
    pr_info("Pool: Created volume '%s' (%llu MB, %s)",
            vol->name, vol->size / MB,
            thin ? "thin" : "thick");
    
    return vol;
}

void volume_destroy(storage_volume_t *vol)
{
    if (!vol) return;
    
    storage_pool_t *pool = vol->pool;
    
    /* Unregister block device */
    block_unregister(&vol->blkdev);
    
    /* Free extents */
    for (uint32_t i = 0; i < vol->num_extents; i++) {
        if (vol->extent_map[i] != 0) {
            extent_info_t *ext = &pool->extents[vol->extent_map[i]];
            /* Free replicas */
            for (uint32_t r = 0; r < ext->replica_count; r++) {
                pool_free_extent(pool, ext->replica_extents[r]);
            }
            pool_free_extent(pool, vol->extent_map[i]);
        }
    }
    
    pool->used_size -= vol->allocated;
    pool->free_size += vol->allocated;
    
    /* Remove from pool */
    storage_volume_t **pp = &pool->volumes;
    while (*pp) {
        if (*pp == vol) {
            *pp = vol->next;
            pool->volume_count--;
            break;
        }
        pp = &(*pp)->next;
    }
    
    pr_info("Pool: Destroyed volume '%s'", vol->name);
    
    kfree(vol->extent_map);
    kfree(vol);
}

int volume_resize(storage_volume_t *vol, uint64_t new_size)
{
    if (!vol) return -1;
    
    uint32_t new_extents = (new_size + POOL_EXTENT_SIZE - 1) / POOL_EXTENT_SIZE;
    
    if (new_extents == vol->num_extents) {
        return 0;
    }
    
    if (new_extents < vol->num_extents) {
        /* Shrink - not implemented */
        return -1;
    }
    
    /* Grow */
    uint32_t *new_map = kmalloc(new_extents * sizeof(uint32_t), GFP_KERNEL | GFP_ZERO);
    if (!new_map) return -1;
    
    memcpy(new_map, vol->extent_map, vol->num_extents * sizeof(uint32_t));
    kfree(vol->extent_map);
    vol->extent_map = new_map;
    
    vol->num_extents = new_extents;
    vol->size = new_extents * POOL_EXTENT_SIZE;
    vol->blkdev.size = vol->size;
    vol->blkdev.num_blocks = vol->size / BLOCK_DEFAULT_SIZE;
    
    pr_info("Pool: Resized volume '%s' to %llu MB", vol->name, vol->size / MB);
    
    return 0;
}

storage_volume_t *volume_snapshot(storage_volume_t *vol, const char *name)
{
    if (!vol) return NULL;
    
    /* Create COW snapshot - simplified implementation */
    storage_volume_t *snap = volume_create(vol->pool, name, vol->size, 
                                            vol->replication, true);
    if (!snap) return NULL;
    
    /* Copy extent map (for COW, would mark as shared) */
    memcpy(snap->extent_map, vol->extent_map, 
           vol->num_extents * sizeof(uint32_t));
    
    pr_info("Pool: Created snapshot '%s' of '%s'", name, vol->name);
    
    return snap;
}

block_device_t *volume_get_block_device(storage_volume_t *vol)
{
    return vol ? &vol->blkdev : NULL;
}
