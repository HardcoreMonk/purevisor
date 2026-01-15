/*
 * PureVisor - Virtio Core Implementation
 * 
 * Virtio device infrastructure and virtqueue management
 */

#include <lib/types.h>
#include <lib/string.h>
#include <virtio/virtio.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Virtqueue Operations
 * ============================================================================ */

void virtq_init(virtqueue_t *vq, uint16_t index, uint16_t size)
{
    memset(vq, 0, sizeof(*vq));
    vq->index = index;
    vq->num = size;
    vq->enabled = false;
}

int virtq_set_addr(virtqueue_t *vq, uint64_t desc, uint64_t avail, uint64_t used)
{
    vq->desc_addr = desc;
    vq->avail_addr = avail;
    vq->used_addr = used;
    
    /* Map guest physical addresses to host virtual */
    vq->desc = (virtq_desc_t *)phys_to_virt(desc);
    vq->avail = (virtq_avail_t *)phys_to_virt(avail);
    vq->used = (virtq_used_t *)phys_to_virt(used);
    
    vq->enabled = true;
    
    pr_info("Virtqueue %u: desc=0x%llx avail=0x%llx used=0x%llx",
            vq->index, desc, avail, used);
    
    return 0;
}

int virtq_pop(virtqueue_t *vq, uint16_t *head)
{
    if (!vq->enabled || !vq->avail) {
        return 0;
    }
    
    /* Memory barrier */
    __asm__ volatile("mfence" ::: "memory");
    
    uint16_t avail_idx = vq->avail->idx;
    
    if (vq->last_avail_idx == avail_idx) {
        return 0;  /* Queue empty */
    }
    
    /* Get next available descriptor */
    uint16_t ring_idx = vq->last_avail_idx % vq->num;
    *head = vq->avail->ring[ring_idx];
    
    vq->last_avail_idx++;
    
    /* Count descriptors in chain */
    int count = 0;
    uint16_t idx = *head;
    while (count < vq->num) {
        count++;
        if (!(vq->desc[idx].flags & VIRTQ_DESC_F_NEXT)) {
            break;
        }
        idx = vq->desc[idx].next;
    }
    
    return count;
}

int virtq_get_desc(virtqueue_t *vq, uint16_t index, virtq_desc_t *desc)
{
    if (!vq->enabled || index >= vq->num) {
        return -1;
    }
    
    *desc = vq->desc[index];
    return 0;
}

void virtq_push(virtqueue_t *vq, uint16_t head, uint32_t len)
{
    if (!vq->enabled || !vq->used) {
        return;
    }
    
    uint16_t ring_idx = vq->used->idx % vq->num;
    vq->used->ring[ring_idx].id = head;
    vq->used->ring[ring_idx].len = len;
    
    /* Memory barrier before updating index */
    __asm__ volatile("mfence" ::: "memory");
    
    vq->used->idx++;
    vq->last_used_idx++;
}

bool virtq_should_notify(virtqueue_t *vq)
{
    if (!vq->enabled || !vq->avail) {
        return false;
    }
    
    if (vq->avail->flags & VIRTQ_AVAIL_F_NO_INTERRUPT) {
        return false;
    }
    
    /* For EVENT_IDX, check used_event */
    if (vq->event_idx) {
        /* Calculate used_event address manually to avoid alignment warning */
        uintptr_t addr = (uintptr_t)&vq->avail->ring[vq->num];
        uint16_t used_event = *(uint16_t *)addr;
        uint16_t new_idx = vq->used->idx;
        uint16_t old_idx = vq->last_used_idx - 1;
        
        return (new_idx - used_event - 1) < (new_idx - old_idx);
    }
    
    return true;
}

void virtq_enable_notify(virtqueue_t *vq)
{
    if (vq->used) {
        vq->used->flags &= ~VIRTQ_USED_F_NO_NOTIFY;
    }
}

void virtq_disable_notify(virtqueue_t *vq)
{
    if (vq->used) {
        vq->used->flags |= VIRTQ_USED_F_NO_NOTIFY;
    }
}

/* ============================================================================
 * Virtio PCI Device
 * ============================================================================ */

/* PCI config space callback */
static int virtio_pci_config_read(pci_device_t *pci, uint8_t offset,
                                   int size, uint32_t *value)
{
    /* Default read from config space */
    *value = 0;
    if (offset + size <= PCI_CONFIG_SPACE_SIZE) {
        memcpy(value, &pci->config[offset], size);
    }
    return 0;
}

static int virtio_pci_config_write(pci_device_t *pci, uint8_t offset,
                                    int size, uint32_t value)
{
    if (offset + size <= PCI_CONFIG_SPACE_SIZE) {
        memcpy(&pci->config[offset], &value, size);
    }
    return 0;
}

/* BAR0 read (I/O space - legacy) */
static int virtio_bar_read(pci_device_t *pci, int bar, uint64_t offset,
                           int size, uint64_t *value)
{
    virtio_device_t *dev = (virtio_device_t *)pci;
    return virtio_pci_read(dev, bar, offset, size, value);
}

/* BAR0 write (I/O space - legacy) */
static int virtio_bar_write(pci_device_t *pci, int bar, uint64_t offset,
                            int size, uint64_t value)
{
    virtio_device_t *dev = (virtio_device_t *)pci;
    return virtio_pci_write(dev, bar, offset, size, value);
}

int virtio_pci_init(virtio_device_t *dev, uint16_t type)
{
    memset(dev, 0, sizeof(*dev));
    
    /* Set PCI identifiers */
    dev->pci.vendor_id = VIRTIO_PCI_VENDOR_ID;
    
    /* Use transitional device IDs for compatibility */
    switch (type) {
        case VIRTIO_SUBSYS_NET:
            dev->pci.device_id = VIRTIO_PCI_DEVICE_NET;
            break;
        case VIRTIO_SUBSYS_BLK:
            dev->pci.device_id = VIRTIO_PCI_DEVICE_BLK;
            break;
        case VIRTIO_SUBSYS_CONSOLE:
            dev->pci.device_id = VIRTIO_PCI_DEVICE_CONSOLE;
            break;
        default:
            dev->pci.device_id = 0x1000 + type;
            break;
    }
    
    dev->pci.subsys_vendor_id = VIRTIO_PCI_VENDOR_ID;
    dev->pci.subsys_id = type;
    dev->pci.revision = 0;
    dev->pci.class_code = PCI_CLASS_MISC;
    dev->pci.subclass = 0;
    dev->pci.prog_if = 0;
    
    dev->device_type = type;
    
    /* Setup I/O BAR for legacy interface */
    pci_setup_bar(&dev->pci, 0, 0xC000, 256, true, false, false);
    
    /* Setup callbacks */
    dev->pci.config_read = virtio_pci_config_read;
    dev->pci.config_write = virtio_pci_config_write;
    dev->pci.bar_read = virtio_bar_read;
    dev->pci.bar_write = virtio_bar_write;
    
    /* Set common features */
    dev->host_features = BIT(VIRTIO_F_VERSION_1) |
                         BIT(VIRTIO_F_RING_EVENT_IDX);
    
    pr_info("Virtio: Initialized PCI device type %u (ID 0x%04x)",
            type, dev->pci.device_id);
    
    return 0;
}

virtqueue_t *virtio_add_queue(virtio_device_t *dev, uint16_t size)
{
    if (dev->num_queues >= VIRTIO_MAX_QUEUES) {
        return NULL;
    }
    
    uint16_t idx = dev->num_queues++;
    virtqueue_t *vq = &dev->queues[idx];
    virtq_init(vq, idx, size);
    
    return vq;
}

void virtio_set_config(virtio_device_t *dev, void *config, size_t size)
{
    dev->config = (uint8_t *)config;
    dev->config_size = size;
}

void virtio_notify_config(virtio_device_t *dev)
{
    dev->isr_status |= 2;  /* Config change bit */
    
    /* Interrupt injection:
     * In a real implementation, this would inject an interrupt to the guest
     * via the VCPU's virtual LAPIC. For now, we set the ISR status and
     * the guest driver polls or receives MSI notification.
     * 
     * Implementation note: When running in VMX root mode with a guest,
     * use vmx_inject_interrupt() to deliver the interrupt.
     */
    if (dev->pci.irq_line != 0) {
        /* MSI/MSI-X or legacy interrupt pending */
        dev->pci.interrupt_pending = true;
    }
}

/* ============================================================================
 * Legacy I/O Space Access
 * ============================================================================ */

int virtio_pci_read(virtio_device_t *dev, int bar, uint64_t offset,
                    int size, uint64_t *value)
{
    if (bar != 0) {
        *value = 0;
        return 0;
    }
    
    *value = 0;
    
    switch (offset) {
        case VIRTIO_PCI_HOST_FEATURES:
            *value = (uint32_t)dev->host_features;
            break;
            
        case VIRTIO_PCI_GUEST_FEATURES:
            *value = (uint32_t)dev->guest_features;
            break;
            
        case VIRTIO_PCI_QUEUE_PFN:
            if (dev->queue_sel < dev->num_queues) {
                *value = dev->queues[dev->queue_sel].desc_addr >> 12;
            }
            break;
            
        case VIRTIO_PCI_QUEUE_NUM:
            if (dev->queue_sel < dev->num_queues) {
                *value = dev->queues[dev->queue_sel].num;
            }
            break;
            
        case VIRTIO_PCI_QUEUE_SEL:
            *value = dev->queue_sel;
            break;
            
        case VIRTIO_PCI_STATUS:
            *value = dev->status;
            break;
            
        case VIRTIO_PCI_ISR:
            *value = dev->isr_status;
            dev->isr_status = 0;  /* Clear on read */
            break;
            
        default:
            /* Device-specific config */
            if (offset >= VIRTIO_PCI_CONFIG && dev->config) {
                uint32_t cfg_offset = offset - VIRTIO_PCI_CONFIG;
                if (cfg_offset + size <= dev->config_size) {
                    memcpy(value, &dev->config[cfg_offset], size);
                }
            }
            break;
    }
    
    return 0;
}

int virtio_pci_write(virtio_device_t *dev, int bar, uint64_t offset,
                     int size, uint64_t value)
{
    if (bar != 0) {
        return 0;
    }
    
    (void)size;
    
    switch (offset) {
        case VIRTIO_PCI_GUEST_FEATURES:
            dev->guest_features = (uint32_t)value;
            break;
            
        case VIRTIO_PCI_QUEUE_PFN:
            if (dev->queue_sel < dev->num_queues) {
                virtqueue_t *vq = &dev->queues[dev->queue_sel];
                uint64_t pfn = (uint32_t)value;
                
                if (pfn == 0) {
                    vq->enabled = false;
                } else {
                    uint64_t addr = pfn << 12;
                    /* Legacy layout: desc, avail, used contiguous */
                    uint64_t desc_addr = addr;
                    uint64_t avail_addr = addr + vq->num * sizeof(virtq_desc_t);
                    uint64_t used_addr = (avail_addr + 4 + vq->num * 2 + 4095) & ~4095ULL;
                    
                    virtq_set_addr(vq, desc_addr, avail_addr, used_addr);
                }
            }
            break;
            
        case VIRTIO_PCI_QUEUE_SEL:
            dev->queue_sel = (uint16_t)value;
            break;
            
        case VIRTIO_PCI_QUEUE_NOTIFY:
            if ((uint16_t)value < dev->num_queues && dev->queue_notify) {
                dev->queue_notify(dev, (uint16_t)value);
            }
            break;
            
        case VIRTIO_PCI_STATUS:
            if ((uint8_t)value == 0) {
                /* Device reset */
                dev->status = 0;
                dev->guest_features = 0;
                dev->isr_status = 0;
                for (int i = 0; i < dev->num_queues; i++) {
                    dev->queues[i].enabled = false;
                    dev->queues[i].last_avail_idx = 0;
                    dev->queues[i].last_used_idx = 0;
                }
                if (dev->reset) {
                    dev->reset(dev);
                }
            } else {
                dev->status = (uint8_t)value;
                
                /* Check FEATURES_OK transition */
                if ((dev->status & VIRTIO_STATUS_FEATURES_OK) && !dev->features_ok) {
                    /* Validate feature negotiation */
                    dev->features_ok = true;
                }
            }
            break;
            
        default:
            /* Device-specific config */
            if (offset >= VIRTIO_PCI_CONFIG && dev->config && dev->config_write) {
                uint32_t cfg_offset = offset - VIRTIO_PCI_CONFIG;
                dev->config_write(dev, cfg_offset, size, value);
            }
            break;
    }
    
    return 0;
}
