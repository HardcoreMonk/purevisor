/*
 * PureVisor - PCI Definitions Header
 * 
 * PCI configuration space and emulation structures
 */

#ifndef _PUREVISOR_PCI_H
#define _PUREVISOR_PCI_H

#include <lib/types.h>

/* ============================================================================
 * PCI Configuration Space
 * ============================================================================ */

#define PCI_CONFIG_ADDR         0xCF8
#define PCI_CONFIG_DATA         0xCFC

/* PCI configuration space registers */
#define PCI_VENDOR_ID           0x00
#define PCI_DEVICE_ID           0x02
#define PCI_COMMAND             0x04
#define PCI_STATUS              0x06
#define PCI_REVISION_ID         0x08
#define PCI_PROG_IF             0x09
#define PCI_SUBCLASS            0x0A
#define PCI_CLASS               0x0B
#define PCI_CACHE_LINE_SIZE     0x0C
#define PCI_LATENCY_TIMER       0x0D
#define PCI_HEADER_TYPE         0x0E
#define PCI_BIST                0x0F
#define PCI_BAR0                0x10
#define PCI_BAR1                0x14
#define PCI_BAR2                0x18
#define PCI_BAR3                0x1C
#define PCI_BAR4                0x20
#define PCI_BAR5                0x24
#define PCI_CARDBUS_CIS         0x28
#define PCI_SUBSYSTEM_VENDOR_ID 0x2C
#define PCI_SUBSYSTEM_ID        0x2E
#define PCI_ROM_ADDRESS         0x30
#define PCI_CAPABILITIES        0x34
#define PCI_INTERRUPT_LINE      0x3C
#define PCI_INTERRUPT_PIN       0x3D
#define PCI_MIN_GNT             0x3E
#define PCI_MAX_LAT             0x3F

/* PCI Command register bits */
#define PCI_CMD_IO_SPACE        BIT(0)
#define PCI_CMD_MEM_SPACE       BIT(1)
#define PCI_CMD_BUS_MASTER      BIT(2)
#define PCI_CMD_SPECIAL_CYCLES  BIT(3)
#define PCI_CMD_MEM_WRITE_INV   BIT(4)
#define PCI_CMD_VGA_PALETTE     BIT(5)
#define PCI_CMD_PARITY_ERROR    BIT(6)
#define PCI_CMD_SERR            BIT(8)
#define PCI_CMD_FAST_B2B        BIT(9)
#define PCI_CMD_INT_DISABLE     BIT(10)

/* PCI Status register bits */
#define PCI_STATUS_INT_STATUS   BIT(3)
#define PCI_STATUS_CAP_LIST     BIT(4)
#define PCI_STATUS_66MHZ        BIT(5)
#define PCI_STATUS_FAST_B2B     BIT(7)
#define PCI_STATUS_PARITY_ERR   BIT(8)
#define PCI_STATUS_DEVSEL_MASK  (3 << 9)
#define PCI_STATUS_SIG_ABORT    BIT(11)
#define PCI_STATUS_RCV_ABORT    BIT(12)
#define PCI_STATUS_RCV_MASTER   BIT(13)
#define PCI_STATUS_SIG_SYSTEM   BIT(14)
#define PCI_STATUS_PARITY_DET   BIT(15)

/* BAR types */
#define PCI_BAR_IO              0x01
#define PCI_BAR_MEM_32          0x00
#define PCI_BAR_MEM_64          0x04
#define PCI_BAR_PREFETCH        0x08

/* Class codes */
#define PCI_CLASS_STORAGE       0x01
#define PCI_CLASS_NETWORK       0x02
#define PCI_CLASS_DISPLAY       0x03
#define PCI_CLASS_MULTIMEDIA    0x04
#define PCI_CLASS_MEMORY        0x05
#define PCI_CLASS_BRIDGE        0x06
#define PCI_CLASS_COMM          0x07
#define PCI_CLASS_SYSTEM        0x08
#define PCI_CLASS_INPUT         0x09
#define PCI_CLASS_DOCKING       0x0A
#define PCI_CLASS_PROCESSOR     0x0B
#define PCI_CLASS_SERIAL        0x0C
#define PCI_CLASS_WIRELESS      0x0D
#define PCI_CLASS_MISC          0xFF

/* Capability IDs */
#define PCI_CAP_POWER_MGMT      0x01
#define PCI_CAP_AGP             0x02
#define PCI_CAP_VPD             0x03
#define PCI_CAP_SLOT_ID         0x04
#define PCI_CAP_MSI             0x05
#define PCI_CAP_PCIX            0x07
#define PCI_CAP_VENDOR          0x09
#define PCI_CAP_PCIE            0x10
#define PCI_CAP_MSIX            0x11

/* ============================================================================
 * MSI/MSI-X Capability
 * ============================================================================ */

#define MSI_CTRL_ENABLE         BIT(0)
#define MSI_CTRL_MULTI_MSG_CAP  (7 << 1)
#define MSI_CTRL_MULTI_MSG_EN   (7 << 4)
#define MSI_CTRL_64BIT          BIT(7)
#define MSI_CTRL_PER_VECTOR     BIT(8)

#define MSIX_CTRL_TABLE_SIZE    0x07FF
#define MSIX_CTRL_FUNC_MASK     BIT(14)
#define MSIX_CTRL_ENABLE        BIT(15)

/* ============================================================================
 * PCI Device Structure
 * ============================================================================ */

#define PCI_CONFIG_SPACE_SIZE   256
#define PCI_MAX_BARS            6

typedef struct pci_bar {
    uint64_t base;
    uint64_t size;
    uint32_t type;
    bool is_io;
    bool is_64bit;
    bool prefetchable;
} pci_bar_t;

typedef struct pci_device {
    /* Location */
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    
    /* Configuration space */
    uint8_t config[PCI_CONFIG_SPACE_SIZE];
    
    /* Parsed BARs */
    pci_bar_t bars[PCI_MAX_BARS];
    
    /* Device info */
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t subsys_vendor_id;
    uint16_t subsys_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t revision;
    
    /* Interrupt */
    uint8_t irq_line;
    uint8_t irq_pin;
    bool interrupt_pending;  /* Interrupt waiting to be delivered */
    
    /* Capabilities */
    bool has_msi;
    bool has_msix;
    uint8_t msi_cap_offset;
    uint8_t msix_cap_offset;
    
    /* Callbacks for device emulation */
    int (*config_read)(struct pci_device *dev, uint8_t offset, 
                       int size, uint32_t *value);
    int (*config_write)(struct pci_device *dev, uint8_t offset,
                        int size, uint32_t value);
    int (*bar_read)(struct pci_device *dev, int bar, uint64_t offset,
                    int size, uint64_t *value);
    int (*bar_write)(struct pci_device *dev, int bar, uint64_t offset,
                     int size, uint64_t value);
    
    /* Device-specific data */
    void *priv;
    
    /* Linked list */
    struct pci_device *next;
} pci_device_t;

/* ============================================================================
 * PCI Bus Structure
 * ============================================================================ */

#define PCI_MAX_DEVICES         32
#define PCI_MAX_FUNCTIONS       8

typedef struct pci_bus {
    pci_device_t *devices;
    uint32_t device_count;
    
    /* Config address register state */
    uint32_t config_address;
} pci_bus_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * pci_init - Initialize PCI emulation
 */
int pci_init(void);

/**
 * pci_register_device - Register a PCI device
 * @dev: Device to register
 */
int pci_register_device(pci_device_t *dev);

/**
 * pci_unregister_device - Remove a PCI device
 * @dev: Device to remove
 */
void pci_unregister_device(pci_device_t *dev);

/**
 * pci_find_device - Find device by vendor/device ID
 */
pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id);

/**
 * pci_config_read - Read PCI config space
 * @bus, device, function: PCI address
 * @offset: Register offset
 * @size: 1, 2, or 4 bytes
 * @value: Output value
 */
int pci_config_read(uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t offset, int size, uint32_t *value);

/**
 * pci_config_write - Write PCI config space
 */
int pci_config_write(uint8_t bus, uint8_t device, uint8_t function,
                     uint8_t offset, int size, uint32_t value);

/**
 * pci_handle_io - Handle PCI I/O port access
 * @port: I/O port (0xCF8 or 0xCFC)
 * @is_write: true for write, false for read
 * @size: Access size
 * @value: Pointer to value
 */
int pci_handle_io(uint16_t port, bool is_write, int size, uint32_t *value);

/**
 * pci_setup_bar - Configure a BAR
 * @dev: Device
 * @bar: BAR index (0-5)
 * @base: Base address
 * @size: Size (must be power of 2)
 * @is_io: true for I/O space, false for memory
 * @is_64bit: true for 64-bit BAR
 * @prefetch: true for prefetchable memory
 */
void pci_setup_bar(pci_device_t *dev, int bar, uint64_t base,
                   uint64_t size, bool is_io, bool is_64bit, bool prefetch);

/**
 * pci_add_capability - Add a capability to device
 * @dev: Device
 * @cap_id: Capability ID
 * @offset: Offset in config space
 * @size: Size of capability data
 */
void pci_add_capability(pci_device_t *dev, uint8_t cap_id, 
                        uint8_t offset, uint8_t size);

#endif /* _PUREVISOR_PCI_H */
