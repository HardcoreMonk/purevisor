/*
 * PureVisor - Virtio Block Device Implementation
 * 
 * Virtual block device emulation
 */

#include <lib/types.h>
#include <lib/string.h>
#include <virtio/virtio.h>
#include <virtio/virtio_blk.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Memory Backend
 * ============================================================================ */

static int memory_read(blk_backend_t *be, uint64_t offset, 
                       void *buf, size_t len)
{
    if (offset + len > be->size) {
        return -1;
    }
    
    uint8_t *data = (uint8_t *)be->data;
    memcpy(buf, data + offset, len);
    return 0;
}

static int memory_write(blk_backend_t *be, uint64_t offset,
                        const void *buf, size_t len)
{
    if (be->readonly || offset + len > be->size) {
        return -1;
    }
    
    uint8_t *data = (uint8_t *)be->data;
    memcpy(data + offset, buf, len);
    return 0;
}

static int memory_flush(blk_backend_t *be UNUSED)
{
    return 0;  /* Memory backend doesn't need flush */
}

blk_backend_t *blk_backend_create_memory(uint64_t size)
{
    blk_backend_t *be = kmalloc(sizeof(blk_backend_t), GFP_KERNEL | GFP_ZERO);
    if (!be) return NULL;
    
    /* Allocate memory for disk */
    uint64_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    phys_addr_t phys = pmm_alloc_pages(pages > 10 ? 10 : pages);  /* Limit for now */
    if (!phys) {
        kfree(be);
        return NULL;
    }
    
    be->type = BLK_BACKEND_MEMORY;
    be->size = pages * PAGE_SIZE;
    be->sector_size = 512;
    be->readonly = false;
    be->data = phys_to_virt(phys);
    
    /* Zero the disk */
    memset(be->data, 0, be->size);
    
    /* Set ID */
    strcpy(be->id, "PureVisor-RAMDisk");
    
    /* Set operations */
    be->read = memory_read;
    be->write = memory_write;
    be->flush = memory_flush;
    
    pr_info("Block: Created RAM disk, size=%llu KB", be->size / 1024);
    
    return be;
}

void blk_backend_destroy(blk_backend_t *be)
{
    if (!be) return;
    
    if (be->type == BLK_BACKEND_MEMORY && be->data) {
        phys_addr_t phys = virt_to_phys(be->data);
        uint64_t pages = (be->size + PAGE_SIZE - 1) / PAGE_SIZE;
        pmm_free_pages(phys, pages > 10 ? 10 : pages);
    }
    
    kfree(be);
}

/* ============================================================================
 * Request Processing
 * ============================================================================ */

static void process_request(virtio_blk_t *blk, virtqueue_t *vq, uint16_t head)
{
    blk_backend_t *be = blk->backend;
    virtq_desc_t desc;
    uint8_t status = VIRTIO_BLK_S_OK;
    uint32_t written = 0;
    
    /* Get request header */
    if (virtq_get_desc(vq, head, &desc) != 0) {
        return;
    }
    
    virtio_blk_req_hdr_t *hdr = (virtio_blk_req_hdr_t *)phys_to_virt(desc.addr);
    uint16_t next = desc.next;
    bool has_next = desc.flags & VIRTQ_DESC_F_NEXT;
    
    switch (hdr->type) {
        case VIRTIO_BLK_T_IN: {
            /* Read request */
            uint64_t sector = hdr->sector;
            
            while (has_next) {
                if (virtq_get_desc(vq, next, &desc) != 0) break;
                
                if (desc.flags & VIRTQ_DESC_F_WRITE) {
                    /* This is a writable buffer - data or status */
                    if (desc.len == 1) {
                        /* Status byte */
                        *(uint8_t *)phys_to_virt(desc.addr) = status;
                        written = 1;
                    } else {
                        /* Data buffer */
                        void *buf = phys_to_virt(desc.addr);
                        uint64_t offset = sector * 512;
                        
                        if (be->read(be, offset, buf, desc.len) != 0) {
                            status = VIRTIO_BLK_S_IOERR;
                        }
                        
                        sector += desc.len / 512;
                        written += desc.len;
                    }
                }
                
                has_next = desc.flags & VIRTQ_DESC_F_NEXT;
                next = desc.next;
            }
            break;
        }
        
        case VIRTIO_BLK_T_OUT: {
            /* Write request */
            uint64_t sector = hdr->sector;
            
            while (has_next) {
                if (virtq_get_desc(vq, next, &desc) != 0) break;
                
                if (desc.flags & VIRTQ_DESC_F_WRITE) {
                    /* Status byte */
                    *(uint8_t *)phys_to_virt(desc.addr) = status;
                    written = 1;
                } else if (desc.len > 1) {
                    /* Data to write */
                    void *buf = phys_to_virt(desc.addr);
                    uint64_t offset = sector * 512;
                    
                    if (be->write(be, offset, buf, desc.len) != 0) {
                        status = VIRTIO_BLK_S_IOERR;
                    }
                    
                    sector += desc.len / 512;
                }
                
                has_next = desc.flags & VIRTQ_DESC_F_NEXT;
                next = desc.next;
            }
            break;
        }
        
        case VIRTIO_BLK_T_FLUSH:
            if (be->flush(be) != 0) {
                status = VIRTIO_BLK_S_IOERR;
            }
            
            /* Find status descriptor */
            while (has_next) {
                if (virtq_get_desc(vq, next, &desc) != 0) break;
                if ((desc.flags & VIRTQ_DESC_F_WRITE) && desc.len == 1) {
                    *(uint8_t *)phys_to_virt(desc.addr) = status;
                    written = 1;
                    break;
                }
                has_next = desc.flags & VIRTQ_DESC_F_NEXT;
                next = desc.next;
            }
            break;
            
        case VIRTIO_BLK_T_GET_ID:
            /* Return disk ID */
            while (has_next) {
                if (virtq_get_desc(vq, next, &desc) != 0) break;
                
                if (desc.flags & VIRTQ_DESC_F_WRITE) {
                    if (desc.len == 1) {
                        *(uint8_t *)phys_to_virt(desc.addr) = status;
                        written = 1;
                    } else {
                        char *buf = (char *)phys_to_virt(desc.addr);
                        size_t len = desc.len < 20 ? desc.len : 20;
                        memcpy(buf, be->id, len);
                        written += len;
                    }
                }
                
                has_next = desc.flags & VIRTQ_DESC_F_NEXT;
                next = desc.next;
            }
            break;
            
        default:
            status = VIRTIO_BLK_S_UNSUPP;
            /* Find status descriptor */
            while (has_next) {
                if (virtq_get_desc(vq, next, &desc) != 0) break;
                if ((desc.flags & VIRTQ_DESC_F_WRITE) && desc.len == 1) {
                    *(uint8_t *)phys_to_virt(desc.addr) = status;
                    written = 1;
                    break;
                }
                has_next = desc.flags & VIRTQ_DESC_F_NEXT;
                next = desc.next;
            }
            break;
    }
    
    virtq_push(vq, head, written);
}

static int blk_queue_notify(virtio_device_t *dev, uint16_t queue)
{
    virtio_blk_t *blk = (virtio_blk_t *)dev;
    
    if (queue >= dev->num_queues) {
        return -1;
    }
    
    virtqueue_t *vq = &dev->queues[queue];
    uint16_t head;
    
    /* Process all available requests */
    while (virtq_pop(vq, &head) > 0) {
        process_request(blk, vq, head);
    }
    
    /* Signal completion */
    if (virtq_should_notify(vq)) {
        dev->isr_status |= 1;
        /* Interrupt injection: ISR status set for guest polling.
         * For VMX guests, inject via vmx_inject_interrupt() when
         * the guest VCPU is scheduled. */
        dev->pci.interrupt_pending = true;
    }
    
    return 0;
}

static void blk_reset(virtio_device_t *dev UNUSED)
{
    /* Nothing special to reset */
}

/* ============================================================================
 * Public API
 * ============================================================================ */

virtio_blk_t *virtio_blk_create(blk_backend_t *backend)
{
    if (!backend) return NULL;
    
    virtio_blk_t *blk = kmalloc(sizeof(virtio_blk_t), GFP_KERNEL | GFP_ZERO);
    if (!blk) return NULL;
    
    /* Initialize virtio device */
    virtio_pci_init(&blk->dev, VIRTIO_SUBSYS_BLK);
    
    blk->backend = backend;
    
    /* Set block-specific features */
    blk->dev.host_features |= BIT(VIRTIO_BLK_F_SEG_MAX) |
                               BIT(VIRTIO_BLK_F_BLK_SIZE) |
                               BIT(VIRTIO_BLK_F_FLUSH);
    
    if (backend->readonly) {
        blk->dev.host_features |= BIT(VIRTIO_BLK_F_RO);
    }
    
    /* Setup config */
    blk->config.capacity = backend->size / 512;
    blk->config.size_max = 4096;
    blk->config.seg_max = 128;
    blk->config.blk_size = backend->sector_size;
    blk->config.num_queues = 1;
    
    virtio_set_config(&blk->dev, &blk->config, sizeof(blk->config));
    
    /* Add request queue */
    virtio_add_queue(&blk->dev, VIRTQ_MAX_SIZE);
    
    /* Set callbacks */
    blk->dev.queue_notify = blk_queue_notify;
    blk->dev.reset = blk_reset;
    
    pr_info("Virtio-blk: Created device, capacity=%llu sectors (%llu MB)",
            blk->config.capacity, blk->config.capacity / 2048);
    
    return blk;
}

void virtio_blk_destroy(virtio_blk_t *blk)
{
    if (!blk) return;
    
    pci_unregister_device(&blk->dev.pci);
    kfree(blk);
}
