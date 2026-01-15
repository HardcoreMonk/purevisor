/*
 * PureVisor - Memory Block Device
 * 
 * RAM-backed block device for testing
 */

#include <lib/types.h>
#include <lib/string.h>
#include <storage/block.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Memory Device Structure
 * ============================================================================ */

typedef struct mem_block_device {
    block_device_t blkdev;
    void *memory;
    uint64_t mem_size;
} mem_block_device_t;

/* ============================================================================
 * Operations
 * ============================================================================ */

static int mem_submit(block_device_t *dev, block_request_t *req)
{
    mem_block_device_t *mdev = (mem_block_device_t *)dev->priv;
    
    if (req->offset + req->length > mdev->mem_size) {
        req->status = -1;
        if (req->completion) req->completion(req->completion_ctx, -1);
        return -1;
    }
    
    int status = 0;
    uint8_t *mem = (uint8_t *)mdev->memory;
    
    switch (req->op) {
        case BLOCK_OP_READ:
            memcpy(req->buffer, mem + req->offset, req->length);
            break;
            
        case BLOCK_OP_WRITE:
            memcpy(mem + req->offset, req->buffer, req->length);
            break;
            
        case BLOCK_OP_FLUSH:
            /* Nothing to do for memory */
            break;
            
        case BLOCK_OP_WRITE_ZEROES:
            memset(mem + req->offset, 0, req->length);
            break;
            
        default:
            status = -1;
            break;
    }
    
    req->status = status;
    if (req->completion) req->completion(req->completion_ctx, status);
    
    return status;
}

static int mem_flush(block_device_t *dev UNUSED)
{
    return 0;
}

static const block_ops_t mem_ops = {
    .submit = mem_submit,
    .flush = mem_flush,
};

/* ============================================================================
 * Public API
 * ============================================================================ */

block_device_t *mem_block_create(const char *name, uint64_t size)
{
    mem_block_device_t *mdev = kmalloc(sizeof(mem_block_device_t), 
                                        GFP_KERNEL | GFP_ZERO);
    if (!mdev) return NULL;
    
    /* Allocate memory */
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    phys_addr_t phys = pmm_alloc_pages(pages > 10 ? 10 : pages);
    if (!phys) {
        kfree(mdev);
        return NULL;
    }
    
    mdev->memory = phys_to_virt(phys);
    mdev->mem_size = pages * PAGE_SIZE;
    memset(mdev->memory, 0, mdev->mem_size);
    
    /* Setup block device */
    strncpy(mdev->blkdev.name, name, BLOCK_MAX_NAME - 1);
    mdev->blkdev.size = mdev->mem_size;
    mdev->blkdev.block_size = BLOCK_DEFAULT_SIZE;
    mdev->blkdev.num_blocks = mdev->mem_size / BLOCK_DEFAULT_SIZE;
    mdev->blkdev.ops = &mem_ops;
    mdev->blkdev.priv = mdev;
    mdev->blkdev.max_queue_depth = 32;
    
    pr_info("MemBlock: Created '%s', %llu MB", name, mdev->mem_size / MB);
    
    return &mdev->blkdev;
}

void mem_block_destroy(block_device_t *dev)
{
    if (!dev) return;
    
    mem_block_device_t *mdev = (mem_block_device_t *)dev->priv;
    
    if (mdev->memory) {
        phys_addr_t phys = virt_to_phys(mdev->memory);
        uint64_t pages = (mdev->mem_size + PAGE_SIZE - 1) / PAGE_SIZE;
        pmm_free_pages(phys, pages > 10 ? 10 : pages);
    }
    
    kfree(mdev);
}
