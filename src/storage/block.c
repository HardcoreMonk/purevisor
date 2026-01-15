/*
 * PureVisor - Block Storage Implementation
 * 
 * Core block storage layer
 */

#include <lib/types.h>
#include <lib/string.h>
#include <storage/block.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static block_device_t *block_devices = NULL;
static uint32_t block_device_count = 0;
static uint32_t next_device_id = 1;
static bool block_initialized = false;

/* ============================================================================
 * UUID Generation
 * ============================================================================ */

void block_generate_uuid(char *uuid)
{
    /* Simple pseudo-random UUID using TSC */
    uint64_t t1 = rdtsc();
    uint64_t t2 = rdtsc();
    
    /* Format: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx */
    snprintf(uuid, BLOCK_MAX_UUID,
             "%08x-%04x-4%03x-%04x-%012llx",
             (uint32_t)(t1 & 0xFFFFFFFF),
             (uint16_t)((t1 >> 32) & 0xFFFF),
             (uint16_t)((t2 >> 48) & 0x0FFF),
             (uint16_t)(0x8000 | ((t2 >> 32) & 0x3FFF)),
             t2 & 0xFFFFFFFFFFFFULL);
}

/* ============================================================================
 * Request Management
 * ============================================================================ */

block_request_t *block_alloc_request(void)
{
    block_request_t *req = kmalloc(sizeof(block_request_t), GFP_KERNEL | GFP_ZERO);
    return req;
}

void block_free_request(block_request_t *req)
{
    if (req) {
        kfree(req);
    }
}

/* ============================================================================
 * Synchronous I/O
 * ============================================================================ */

static volatile int sync_complete;
static int sync_status;

static void sync_completion(void *ctx UNUSED, int status)
{
    sync_status = status;
    sync_complete = 1;
}

int block_read(block_device_t *dev, uint64_t offset, void *buf, uint32_t len)
{
    if (!dev || !dev->ops || !dev->ops->submit) {
        return -1;
    }
    
    if (offset + len > dev->size) {
        return -1;
    }
    
    block_request_t *req = block_alloc_request();
    if (!req) return -1;
    
    req->op = BLOCK_OP_READ;
    req->offset = offset;
    req->length = len;
    req->buffer = buf;
    req->completion = sync_completion;
    req->flags = BLOCK_REQ_SYNC;
    
    sync_complete = 0;
    
    int ret = dev->ops->submit(dev, req);
    if (ret != 0) {
        block_free_request(req);
        return ret;
    }
    
    /* Wait for completion */
    while (!sync_complete) {
        __asm__ volatile("pause" ::: "memory");
    }
    
    ret = sync_status;
    
    if (ret == 0) {
        dev->read_ops++;
        dev->read_bytes += len;
    } else {
        dev->errors++;
    }
    
    block_free_request(req);
    return ret;
}

int block_write(block_device_t *dev, uint64_t offset, const void *buf, uint32_t len)
{
    if (!dev || !dev->ops || !dev->ops->submit) {
        return -1;
    }
    
    if (dev->readonly) {
        return -1;
    }
    
    if (offset + len > dev->size) {
        return -1;
    }
    
    block_request_t *req = block_alloc_request();
    if (!req) return -1;
    
    req->op = BLOCK_OP_WRITE;
    req->offset = offset;
    req->length = len;
    req->buffer = (void *)buf;
    req->completion = sync_completion;
    req->flags = BLOCK_REQ_SYNC;
    
    sync_complete = 0;
    
    int ret = dev->ops->submit(dev, req);
    if (ret != 0) {
        block_free_request(req);
        return ret;
    }
    
    while (!sync_complete) {
        __asm__ volatile("pause" ::: "memory");
    }
    
    ret = sync_status;
    
    if (ret == 0) {
        dev->write_ops++;
        dev->write_bytes += len;
    } else {
        dev->errors++;
    }
    
    block_free_request(req);
    return ret;
}

int block_flush(block_device_t *dev)
{
    if (!dev || !dev->ops) {
        return -1;
    }
    
    if (dev->ops->flush) {
        return dev->ops->flush(dev);
    }
    
    return 0;
}

int block_submit_async(block_device_t *dev, block_request_t *req)
{
    if (!dev || !dev->ops || !dev->ops->submit || !req) {
        return -1;
    }
    
    return dev->ops->submit(dev, req);
}

/* ============================================================================
 * Device Management
 * ============================================================================ */

int block_init(void)
{
    if (block_initialized) return 0;
    
    pr_info("Block: Initializing storage subsystem");
    
    block_devices = NULL;
    block_device_count = 0;
    next_device_id = 1;
    block_initialized = true;
    
    pr_info("Block: Initialization complete");
    return 0;
}

int block_register(block_device_t *dev)
{
    if (!block_initialized || !dev) {
        return -1;
    }
    
    /* Assign ID */
    dev->id = next_device_id++;
    
    /* Generate UUID if not set */
    if (dev->uuid[0] == '\0') {
        block_generate_uuid(dev->uuid);
    }
    
    /* Calculate num_blocks */
    if (dev->block_size > 0) {
        dev->num_blocks = dev->size / dev->block_size;
    }
    
    /* Initialize queue */
    dev->queue_head = NULL;
    dev->queue_tail = NULL;
    dev->queue_depth = 0;
    if (dev->max_queue_depth == 0) {
        dev->max_queue_depth = 32;
    }
    
    /* Add to list */
    dev->next = block_devices;
    block_devices = dev;
    block_device_count++;
    
    /* Open device */
    if (dev->ops && dev->ops->open) {
        dev->ops->open(dev);
    }
    dev->online = true;
    
    pr_info("Block: Registered %s (%s), %llu MB",
            dev->name, dev->uuid, dev->size / MB);
    
    return 0;
}

void block_unregister(block_device_t *dev)
{
    if (!dev) return;
    
    /* Close device */
    if (dev->ops && dev->ops->close) {
        dev->ops->close(dev);
    }
    dev->online = false;
    
    /* Remove from list */
    block_device_t **pp = &block_devices;
    while (*pp) {
        if (*pp == dev) {
            *pp = dev->next;
            block_device_count--;
            pr_info("Block: Unregistered %s", dev->name);
            return;
        }
        pp = &(*pp)->next;
    }
}

block_device_t *block_find_by_name(const char *name)
{
    block_device_t *dev = block_devices;
    while (dev) {
        if (strcmp(dev->name, name) == 0) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

block_device_t *block_find_by_uuid(const char *uuid)
{
    block_device_t *dev = block_devices;
    while (dev) {
        if (strcmp(dev->uuid, uuid) == 0) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}
