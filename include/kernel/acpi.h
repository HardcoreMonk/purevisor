/*
 * PureVisor - ACPI Header
 * 
 * ACPI table parsing for hardware discovery
 */

#ifndef _PUREVISOR_ACPI_H
#define _PUREVISOR_ACPI_H

#include <lib/types.h>

/* ============================================================================
 * ACPI Table Signatures
 * ============================================================================ */

#define ACPI_SIG_RSDP       "RSD PTR "      /* Root System Description Pointer */
#define ACPI_SIG_RSDT       "RSDT"          /* Root System Description Table */
#define ACPI_SIG_XSDT       "XSDT"          /* Extended System Description Table */
#define ACPI_SIG_MADT       "APIC"          /* Multiple APIC Description Table */
#define ACPI_SIG_FADT       "FACP"          /* Fixed ACPI Description Table */
#define ACPI_SIG_HPET       "HPET"          /* High Precision Event Timer */
#define ACPI_SIG_MCFG       "MCFG"          /* PCI Express Memory Mapped Config */

/* ============================================================================
 * ACPI Structures
 * ============================================================================ */

/* RSDP (Root System Description Pointer) - ACPI 1.0 */
typedef struct PACKED {
    char signature[8];          /* "RSD PTR " */
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;           /* 0 = ACPI 1.0, 2 = ACPI 2.0+ */
    uint32_t rsdt_address;      /* Physical address of RSDT */
} acpi_rsdp_t;

/* RSDP (Extended) - ACPI 2.0+ */
typedef struct PACKED {
    acpi_rsdp_t v1;             /* ACPI 1.0 portion */
    uint32_t length;            /* Length of entire RSDP */
    uint64_t xsdt_address;      /* Physical address of XSDT */
    uint8_t extended_checksum;
    uint8_t reserved[3];
} acpi_rsdp2_t;

/* Standard ACPI Table Header */
typedef struct PACKED {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_header_t;

/* RSDT (Root System Description Table) */
typedef struct PACKED {
    acpi_header_t header;
    uint32_t entries[];         /* Array of 32-bit physical addresses */
} acpi_rsdt_t;

/* XSDT (Extended System Description Table) */
typedef struct PACKED {
    acpi_header_t header;
    uint64_t entries[];         /* Array of 64-bit physical addresses */
} acpi_xsdt_t;

/* ============================================================================
 * MADT (Multiple APIC Description Table)
 * ============================================================================ */

typedef struct PACKED {
    acpi_header_t header;
    uint32_t lapic_address;     /* Physical address of Local APIC */
    uint32_t flags;             /* Flags (bit 0 = PC-AT dual 8259 present) */
    /* Variable length entries follow */
} acpi_madt_t;

/* MADT Entry Types */
#define MADT_TYPE_LAPIC             0   /* Processor Local APIC */
#define MADT_TYPE_IOAPIC            1   /* I/O APIC */
#define MADT_TYPE_ISO               2   /* Interrupt Source Override */
#define MADT_TYPE_NMI_SOURCE        3   /* NMI Source */
#define MADT_TYPE_LAPIC_NMI         4   /* Local APIC NMI */
#define MADT_TYPE_LAPIC_OVERRIDE    5   /* Local APIC Address Override */
#define MADT_TYPE_X2APIC            9   /* Processor Local x2APIC */

/* MADT Entry Header */
typedef struct PACKED {
    uint8_t type;
    uint8_t length;
} madt_entry_header_t;

/* MADT Local APIC Entry */
typedef struct PACKED {
    madt_entry_header_t header;
    uint8_t processor_id;       /* ACPI Processor UID */
    uint8_t apic_id;            /* Local APIC ID */
    uint32_t flags;             /* Bit 0 = Processor Enabled */
} madt_lapic_t;

#define MADT_LAPIC_ENABLED      BIT(0)
#define MADT_LAPIC_ONLINE_CAP   BIT(1)

/* MADT I/O APIC Entry */
typedef struct PACKED {
    madt_entry_header_t header;
    uint8_t ioapic_id;          /* I/O APIC ID */
    uint8_t reserved;
    uint32_t ioapic_address;    /* Physical address */
    uint32_t gsi_base;          /* Global System Interrupt base */
} madt_ioapic_t;

/* MADT Interrupt Source Override Entry */
typedef struct PACKED {
    madt_entry_header_t header;
    uint8_t bus;                /* Always 0 (ISA) */
    uint8_t source;             /* IRQ source */
    uint32_t gsi;               /* Global System Interrupt */
    uint16_t flags;             /* MPS INTI flags */
} madt_iso_t;

/* MADT Local APIC NMI Entry */
typedef struct PACKED {
    madt_entry_header_t header;
    uint8_t processor_id;       /* 0xFF = all processors */
    uint16_t flags;
    uint8_t lint;               /* LINT# (0 or 1) */
} madt_lapic_nmi_t;

/* MADT Local APIC Address Override Entry */
typedef struct PACKED {
    madt_entry_header_t header;
    uint16_t reserved;
    uint64_t lapic_address;     /* 64-bit physical address */
} madt_lapic_override_t;

/* MADT Local x2APIC Entry */
typedef struct PACKED {
    madt_entry_header_t header;
    uint16_t reserved;
    uint32_t x2apic_id;         /* x2APIC ID */
    uint32_t flags;
    uint32_t processor_uid;     /* ACPI Processor UID */
} madt_x2apic_t;

/* ============================================================================
 * ACPI Info Structure (parsed data)
 * ============================================================================ */

typedef struct {
    /* APIC configuration */
    phys_addr_t lapic_address;
    phys_addr_t ioapic_address;
    uint32_t ioapic_gsi_base;
    
    /* CPU information */
    uint32_t cpu_count;
    uint8_t cpu_apic_ids[MAX_CPUS];
    bool cpu_enabled[MAX_CPUS];
    
    /* IRQ overrides */
    struct {
        uint8_t source;
        uint32_t gsi;
        uint16_t flags;
    } irq_overrides[16];
    uint32_t irq_override_count;
    
    /* NMI configuration */
    uint8_t nmi_lint;
    uint16_t nmi_flags;
    
    /* Flags */
    bool has_8259;              /* Dual 8259 PIC present */
    bool has_x2apic;            /* x2APIC present */
} acpi_info_t;

/* ============================================================================
 * Function Declarations
 * ============================================================================ */

/**
 * acpi_init - Initialize ACPI subsystem
 * @rsdp_addr: Physical address of RSDP (from bootloader)
 * 
 * Returns 0 on success, -1 on failure
 */
int acpi_init(phys_addr_t rsdp_addr);

/**
 * acpi_find_table - Find an ACPI table by signature
 * @signature: 4-character table signature
 * 
 * Returns table virtual address or NULL
 */
void *acpi_find_table(const char *signature);

/**
 * acpi_get_info - Get parsed ACPI information
 * 
 * Returns ACPI info structure
 */
const acpi_info_t *acpi_get_info(void);

/**
 * acpi_parse_madt - Parse MADT table
 * @madt: MADT table address
 * 
 * Returns 0 on success, -1 on failure
 */
int acpi_parse_madt(acpi_madt_t *madt);

/**
 * acpi_get_irq_override - Get IRQ override for ISA IRQ
 * @irq: ISA IRQ number
 * @gsi: Output GSI
 * @flags: Output flags
 * 
 * Returns true if override exists
 */
bool acpi_get_irq_override(uint8_t irq, uint32_t *gsi, uint16_t *flags);

/**
 * acpi_dump_info - Print ACPI information
 */
void acpi_dump_info(void);

#endif /* _PUREVISOR_ACPI_H */
