/*
 * PureVisor - Virtio Definitions Header
 * 
 * Virtio specification v1.1+ structures and constants
 */

#ifndef _PUREVISOR_VIRTIO_H
#define _PUREVISOR_VIRTIO_H

#include <lib/types.h>
#include <pci/pci.h>

/* ============================================================================
 * Virtio PCI Vendor/Device IDs
 * ============================================================================ */

#define VIRTIO_PCI_VENDOR_ID        0x1AF4

/* Transitional device IDs (0x1000-0x103F) */
#define VIRTIO_PCI_DEVICE_NET       0x1000
#define VIRTIO_PCI_DEVICE_BLK       0x1001
#define VIRTIO_PCI_DEVICE_CONSOLE   0x1003
#define VIRTIO_PCI_DEVICE_RNG       0x1005
#define VIRTIO_PCI_DEVICE_9P        0x1009

/* Modern device IDs (0x1040+) */
#define VIRTIO_PCI_DEVICE_NET_MODERN    0x1041
#define VIRTIO_PCI_DEVICE_BLK_MODERN    0x1042
#define VIRTIO_PCI_DEVICE_CONSOLE_MODERN 0x1043

/* Subsystem device IDs */
#define VIRTIO_SUBSYS_NET           1
#define VIRTIO_SUBSYS_BLK           2
#define VIRTIO_SUBSYS_CONSOLE       3
#define VIRTIO_SUBSYS_RNG           4
#define VIRTIO_SUBSYS_9P            9

/* ============================================================================
 * Virtio Device Status
 * ============================================================================ */

#define VIRTIO_STATUS_ACKNOWLEDGE   1
#define VIRTIO_STATUS_DRIVER        2
#define VIRTIO_STATUS_DRIVER_OK     4
#define VIRTIO_STATUS_FEATURES_OK   8
#define VIRTIO_STATUS_NEEDS_RESET   64
#define VIRTIO_STATUS_FAILED        128

/* ============================================================================
 * Virtio Feature Bits (common)
 * ============================================================================ */

#define VIRTIO_F_NOTIFY_ON_EMPTY    24
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_F_RING_INDIRECT_DESC 28
#define VIRTIO_F_RING_EVENT_IDX     29
#define VIRTIO_F_VERSION_1          32
#define VIRTIO_F_ACCESS_PLATFORM    33
#define VIRTIO_F_RING_PACKED        34
#define VIRTIO_F_IN_ORDER           35
#define VIRTIO_F_ORDER_PLATFORM     36
#define VIRTIO_F_SR_IOV             37
#define VIRTIO_F_NOTIFICATION_DATA  38

/* ============================================================================
 * Virtio PCI Configuration
 * ============================================================================ */

/* Legacy I/O space offsets */
#define VIRTIO_PCI_HOST_FEATURES    0x00
#define VIRTIO_PCI_GUEST_FEATURES   0x04
#define VIRTIO_PCI_QUEUE_PFN        0x08
#define VIRTIO_PCI_QUEUE_NUM        0x0C
#define VIRTIO_PCI_QUEUE_SEL        0x0E
#define VIRTIO_PCI_QUEUE_NOTIFY     0x10
#define VIRTIO_PCI_STATUS           0x12
#define VIRTIO_PCI_ISR              0x13
#define VIRTIO_PCI_CONFIG           0x14

/* Modern PCI capability types */
#define VIRTIO_PCI_CAP_COMMON_CFG   1
#define VIRTIO_PCI_CAP_NOTIFY_CFG   2
#define VIRTIO_PCI_CAP_ISR_CFG      3
#define VIRTIO_PCI_CAP_DEVICE_CFG   4
#define VIRTIO_PCI_CAP_PCI_CFG      5

/* ============================================================================
 * Virtqueue Structures
 * ============================================================================ */

#define VIRTQ_DESC_F_NEXT       1
#define VIRTQ_DESC_F_WRITE      2
#define VIRTQ_DESC_F_INDIRECT   4

#define VIRTQ_AVAIL_F_NO_INTERRUPT  1
#define VIRTQ_USED_F_NO_NOTIFY      1

#define VIRTQ_MAX_SIZE          256

/* Descriptor table entry */
typedef struct PACKED {
    uint64_t addr;      /* Guest physical address */
    uint32_t len;       /* Length */
    uint16_t flags;     /* VIRTQ_DESC_F_* */
    uint16_t next;      /* Next descriptor if NEXT flag set */
} virtq_desc_t;

/* Available ring */
typedef struct PACKED {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];    /* Descriptor indices */
    /* uint16_t used_event; at end if EVENT_IDX */
} virtq_avail_t;

/* Used ring element */
typedef struct PACKED {
    uint32_t id;        /* Descriptor chain head index */
    uint32_t len;       /* Bytes written to descriptor */
} virtq_used_elem_t;

/* Used ring */
typedef struct PACKED {
    uint16_t flags;
    uint16_t idx;
    virtq_used_elem_t ring[];
    /* uint16_t avail_event; at end if EVENT_IDX */
} virtq_used_t;

/* ============================================================================
 * Virtqueue Structure
 * ============================================================================ */

typedef struct virtqueue {
    /* Queue properties */
    uint16_t num;           /* Queue size */
    uint16_t index;         /* Queue index */
    bool enabled;
    bool event_idx;
    
    /* Guest physical addresses */
    uint64_t desc_addr;
    uint64_t avail_addr;
    uint64_t used_addr;
    
    /* Host virtual addresses (mapped) */
    virtq_desc_t *desc;
    virtq_avail_t *avail;
    virtq_used_t *used;
    
    /* Shadow indices */
    uint16_t last_avail_idx;
    uint16_t last_used_idx;
    
    /* Notification */
    uint16_t notify_offset;
    bool notification_pending;
    
    /* Interrupt injection callback */
    void (*interrupt)(struct virtqueue *vq);
    void *interrupt_data;
} virtqueue_t;

/* ============================================================================
 * Virtio Device Structure
 * ============================================================================ */

#define VIRTIO_MAX_QUEUES   8

typedef struct virtio_device {
    /* PCI device */
    pci_device_t pci;
    
    /* Device type */
    uint16_t device_type;
    
    /* Status */
    uint8_t status;
    uint8_t isr_status;
    
    /* Features */
    uint64_t host_features;
    uint64_t guest_features;
    bool features_ok;
    
    /* Queues */
    virtqueue_t queues[VIRTIO_MAX_QUEUES];
    uint16_t num_queues;
    uint16_t queue_sel;     /* Currently selected queue */
    
    /* Device-specific config space */
    uint8_t *config;
    size_t config_size;
    
    /* Callbacks */
    int (*queue_notify)(struct virtio_device *dev, uint16_t queue);
    int (*config_write)(struct virtio_device *dev, uint32_t offset,
                        int size, uint64_t value);
    void (*reset)(struct virtio_device *dev);
    
    /* Device-specific data */
    void *priv;
} virtio_device_t;

/* ============================================================================
 * Virtqueue Operations
 * ============================================================================ */

/**
 * virtq_init - Initialize a virtqueue
 * @vq: Virtqueue structure
 * @index: Queue index
 * @size: Queue size
 */
void virtq_init(virtqueue_t *vq, uint16_t index, uint16_t size);

/**
 * virtq_set_addr - Set virtqueue addresses (from guest)
 * @vq: Virtqueue
 * @desc, avail, used: Guest physical addresses
 */
int virtq_set_addr(virtqueue_t *vq, uint64_t desc, uint64_t avail, uint64_t used);

/**
 * virtq_pop - Get next available descriptor chain
 * @vq: Virtqueue
 * @head: Output head index
 * 
 * Returns number of descriptors or 0 if empty
 */
int virtq_pop(virtqueue_t *vq, uint16_t *head);

/**
 * virtq_get_desc - Get descriptor by index
 * @vq: Virtqueue
 * @index: Descriptor index
 * @desc: Output descriptor
 */
int virtq_get_desc(virtqueue_t *vq, uint16_t index, virtq_desc_t *desc);

/**
 * virtq_push - Add completed descriptor to used ring
 * @vq: Virtqueue
 * @head: Descriptor chain head
 * @len: Bytes written
 */
void virtq_push(virtqueue_t *vq, uint16_t head, uint32_t len);

/**
 * virtq_notify - Check if guest should be notified
 * @vq: Virtqueue
 */
bool virtq_should_notify(virtqueue_t *vq);

/**
 * virtq_enable_notify - Enable notifications from guest
 * @vq: Virtqueue
 */
void virtq_enable_notify(virtqueue_t *vq);

/**
 * virtq_disable_notify - Disable notifications from guest
 * @vq: Virtqueue
 */
void virtq_disable_notify(virtqueue_t *vq);

/* ============================================================================
 * Virtio Device Operations
 * ============================================================================ */

/**
 * virtio_pci_init - Initialize virtio PCI device
 * @dev: Virtio device
 * @type: Device type (VIRTIO_SUBSYS_*)
 */
int virtio_pci_init(virtio_device_t *dev, uint16_t type);

/**
 * virtio_add_queue - Add a virtqueue to device
 * @dev: Device
 * @size: Queue size
 * @notify: Queue notification callback
 */
virtqueue_t *virtio_add_queue(virtio_device_t *dev, uint16_t size);

/**
 * virtio_set_config - Set device configuration
 * @dev: Device
 * @config: Config data
 * @size: Config size
 */
void virtio_set_config(virtio_device_t *dev, void *config, size_t size);

/**
 * virtio_notify_config - Notify guest of config change
 * @dev: Device
 */
void virtio_notify_config(virtio_device_t *dev);

/**
 * virtio_pci_read - Handle BAR read
 * @dev: Device
 * @bar: BAR index
 * @offset: Offset within BAR
 * @size: Access size
 * @value: Output value
 */
int virtio_pci_read(virtio_device_t *dev, int bar, uint64_t offset,
                    int size, uint64_t *value);

/**
 * virtio_pci_write - Handle BAR write
 * @dev: Device
 * @bar: BAR index
 * @offset: Offset within BAR
 * @size: Access size
 * @value: Value to write
 */
int virtio_pci_write(virtio_device_t *dev, int bar, uint64_t offset,
                     int size, uint64_t value);

#endif /* _PUREVISOR_VIRTIO_H */
