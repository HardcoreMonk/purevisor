/*
 * PureVisor - PCI Emulation Implementation
 * 
 * Virtual PCI bus for guest devices
 */

#include <lib/types.h>
#include <lib/string.h>
#include <pci/pci.h>
#include <mm/heap.h>
#include <kernel/console.h>

/* ============================================================================
 * Global State
 * ============================================================================ */

static pci_bus_t pci_bus;
static bool pci_initialized = false;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline uint32_t make_config_addr(uint8_t bus, uint8_t device,
                                        uint8_t function, uint8_t offset)
{
    return 0x80000000 |
           ((uint32_t)bus << 16) |
           ((uint32_t)device << 11) |
           ((uint32_t)function << 8) |
           (offset & 0xFC);
}

static inline void parse_config_addr(uint32_t addr, uint8_t *bus,
                                     uint8_t *device, uint8_t *function,
                                     uint8_t *offset)
{
    *bus = (addr >> 16) & 0xFF;
    *device = (addr >> 11) & 0x1F;
    *function = (addr >> 8) & 0x07;
    *offset = addr & 0xFF;
}

static pci_device_t *find_device_by_bdf(uint8_t bus, uint8_t device,
                                         uint8_t function)
{
    pci_device_t *dev = pci_bus.devices;
    while (dev) {
        if (dev->bus == bus && dev->device == device && 
            dev->function == function) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

/* ============================================================================
 * Configuration Space Access
 * ============================================================================ */

static uint32_t config_read_default(pci_device_t *dev, uint8_t offset, int size)
{
    uint32_t value = 0;
    
    if (offset + size > PCI_CONFIG_SPACE_SIZE) {
        return 0xFFFFFFFF;
    }
    
    memcpy(&value, &dev->config[offset], size);
    return value;
}

static void config_write_default(pci_device_t *dev, uint8_t offset,
                                 int size, uint32_t value)
{
    if (offset + size > PCI_CONFIG_SPACE_SIZE) {
        return;
    }
    
    /* Handle read-only registers */
    switch (offset) {
        case PCI_VENDOR_ID:
        case PCI_DEVICE_ID:
        case PCI_REVISION_ID:
        case PCI_PROG_IF:
        case PCI_SUBCLASS:
        case PCI_CLASS:
        case PCI_HEADER_TYPE:
        case PCI_SUBSYSTEM_VENDOR_ID:
        case PCI_SUBSYSTEM_ID:
        case PCI_CAPABILITIES:
            /* Read-only */
            return;
            
        case PCI_STATUS:
            /* W1C bits */
            value = dev->config[offset] & ~(value & 0xF9);
            break;
            
        case PCI_COMMAND:
            /* Only allow valid bits */
            value &= 0x07FF;
            break;
    }
    
    /* Handle BAR writes */
    if (offset >= PCI_BAR0 && offset <= PCI_BAR5) {
        int bar = (offset - PCI_BAR0) / 4;
        
        if (dev->bars[bar].size == 0) {
            return;  /* No BAR configured */
        }
        
        if (value == 0xFFFFFFFF) {
            /* BAR sizing - return size mask */
            uint32_t mask = ~(dev->bars[bar].size - 1);
            if (dev->bars[bar].is_io) {
                mask |= 0x01;
            }
            memcpy(&dev->config[offset], &mask, 4);
            return;
        }
        
        /* Normal BAR write - update base */
        if (dev->bars[bar].is_io) {
            value = (value & ~0x03) | 0x01;
        } else {
            value &= ~0x0F;
            if (dev->bars[bar].prefetchable) value |= 0x08;
            if (dev->bars[bar].is_64bit) value |= 0x04;
        }
        dev->bars[bar].base = value & ~(dev->bars[bar].size - 1);
    }
    
    memcpy(&dev->config[offset], &value, size);
}

/* ============================================================================
 * Public API
 * ============================================================================ */

int pci_init(void)
{
    pr_info("PCI: Initializing virtual PCI bus");
    
    memset(&pci_bus, 0, sizeof(pci_bus));
    pci_initialized = true;
    
    pr_info("PCI: Initialization complete");
    return 0;
}

int pci_register_device(pci_device_t *dev)
{
    if (!pci_initialized || !dev) {
        return -1;
    }
    
    /* Initialize config space header */
    memset(dev->config, 0, PCI_CONFIG_SPACE_SIZE);
    
    /* Set standard header fields */
    *(uint16_t *)&dev->config[PCI_VENDOR_ID] = dev->vendor_id;
    *(uint16_t *)&dev->config[PCI_DEVICE_ID] = dev->device_id;
    dev->config[PCI_REVISION_ID] = dev->revision;
    dev->config[PCI_PROG_IF] = dev->prog_if;
    dev->config[PCI_SUBCLASS] = dev->subclass;
    dev->config[PCI_CLASS] = dev->class_code;
    dev->config[PCI_HEADER_TYPE] = 0x00;  /* Normal device */
    *(uint16_t *)&dev->config[PCI_SUBSYSTEM_VENDOR_ID] = dev->subsys_vendor_id;
    *(uint16_t *)&dev->config[PCI_SUBSYSTEM_ID] = dev->subsys_id;
    dev->config[PCI_INTERRUPT_LINE] = dev->irq_line;
    dev->config[PCI_INTERRUPT_PIN] = dev->irq_pin;
    
    /* Set status - capabilities list present */
    *(uint16_t *)&dev->config[PCI_STATUS] = PCI_STATUS_CAP_LIST;
    
    /* Initialize BARs in config space */
    for (int i = 0; i < PCI_MAX_BARS; i++) {
        if (dev->bars[i].size > 0) {
            uint32_t bar_value = dev->bars[i].base;
            if (dev->bars[i].is_io) {
                bar_value |= PCI_BAR_IO;
            } else {
                if (dev->bars[i].is_64bit) bar_value |= PCI_BAR_MEM_64;
                if (dev->bars[i].prefetchable) bar_value |= PCI_BAR_PREFETCH;
            }
            *(uint32_t *)&dev->config[PCI_BAR0 + i * 4] = bar_value;
        }
    }
    
    /* Add to device list */
    dev->next = pci_bus.devices;
    pci_bus.devices = dev;
    pci_bus.device_count++;
    
    pr_info("PCI: Registered device %04x:%04x at %02x:%02x.%x",
            dev->vendor_id, dev->device_id,
            dev->bus, dev->device, dev->function);
    
    return 0;
}

void pci_unregister_device(pci_device_t *dev)
{
    if (!dev) return;
    
    pci_device_t **pp = &pci_bus.devices;
    while (*pp) {
        if (*pp == dev) {
            *pp = dev->next;
            pci_bus.device_count--;
            pr_info("PCI: Unregistered device %04x:%04x",
                    dev->vendor_id, dev->device_id);
            return;
        }
        pp = &(*pp)->next;
    }
}

pci_device_t *pci_find_device(uint16_t vendor_id, uint16_t device_id)
{
    pci_device_t *dev = pci_bus.devices;
    while (dev) {
        if (dev->vendor_id == vendor_id && dev->device_id == device_id) {
            return dev;
        }
        dev = dev->next;
    }
    return NULL;
}

int pci_config_read(uint8_t bus, uint8_t device, uint8_t function,
                    uint8_t offset, int size, uint32_t *value)
{
    pci_device_t *dev = find_device_by_bdf(bus, device, function);
    
    if (!dev) {
        *value = 0xFFFFFFFF;  /* No device */
        return 0;
    }
    
    if (dev->config_read) {
        return dev->config_read(dev, offset, size, value);
    }
    
    *value = config_read_default(dev, offset, size);
    return 0;
}

int pci_config_write(uint8_t bus, uint8_t device, uint8_t function,
                     uint8_t offset, int size, uint32_t value)
{
    pci_device_t *dev = find_device_by_bdf(bus, device, function);
    
    if (!dev) {
        return 0;  /* Ignore writes to non-existent devices */
    }
    
    if (dev->config_write) {
        return dev->config_write(dev, offset, size, value);
    }
    
    config_write_default(dev, offset, size, value);
    return 0;
}

int pci_handle_io(uint16_t port, bool is_write, int size, uint32_t *value)
{
    if (port == PCI_CONFIG_ADDR) {
        if (is_write) {
            pci_bus.config_address = *value;
        } else {
            *value = pci_bus.config_address;
        }
        return 0;
    }
    
    if (port >= PCI_CONFIG_DATA && port < PCI_CONFIG_DATA + 4) {
        if (!(pci_bus.config_address & 0x80000000)) {
            /* Enable bit not set */
            if (!is_write) *value = 0xFFFFFFFF;
            return 0;
        }
        
        uint8_t bus, device, function, offset;
        parse_config_addr(pci_bus.config_address, &bus, &device, 
                         &function, &offset);
        offset += (port - PCI_CONFIG_DATA);
        
        if (is_write) {
            return pci_config_write(bus, device, function, offset, size, *value);
        } else {
            return pci_config_read(bus, device, function, offset, size, value);
        }
    }
    
    return -1;  /* Unknown port */
}

void pci_setup_bar(pci_device_t *dev, int bar, uint64_t base,
                   uint64_t size, bool is_io, bool is_64bit, bool prefetch)
{
    if (!dev || bar < 0 || bar >= PCI_MAX_BARS) return;
    
    dev->bars[bar].base = base;
    dev->bars[bar].size = size;
    dev->bars[bar].is_io = is_io;
    dev->bars[bar].is_64bit = is_64bit;
    dev->bars[bar].prefetchable = prefetch;
    
    /* For 64-bit BAR, mark next BAR as used */
    if (is_64bit && bar < PCI_MAX_BARS - 1) {
        dev->bars[bar + 1].size = 0;  /* Upper 32 bits */
    }
}

void pci_add_capability(pci_device_t *dev, uint8_t cap_id,
                        uint8_t offset, uint8_t size)
{
    if (!dev || offset < 0x40 || offset + size > 0xFF) return;
    
    /* Find end of capability list */
    uint8_t cap_ptr = dev->config[PCI_CAPABILITIES];
    uint8_t prev_offset = PCI_CAPABILITIES;
    
    while (cap_ptr != 0) {
        prev_offset = cap_ptr + 1;  /* Next pointer offset */
        cap_ptr = dev->config[cap_ptr + 1];
    }
    
    /* Link new capability */
    if (prev_offset == PCI_CAPABILITIES) {
        dev->config[PCI_CAPABILITIES] = offset;
    } else {
        dev->config[prev_offset] = offset;
    }
    
    /* Set capability header */
    dev->config[offset] = cap_id;
    dev->config[offset + 1] = 0;  /* Next = NULL */
    
    /* Track MSI/MSI-X */
    if (cap_id == PCI_CAP_MSI) {
        dev->has_msi = true;
        dev->msi_cap_offset = offset;
    } else if (cap_id == PCI_CAP_MSIX) {
        dev->has_msix = true;
        dev->msix_cap_offset = offset;
    }
}
