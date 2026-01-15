/*
 * PureVisor - Extended Page Tables (EPT) Header
 * 
 * Intel EPT for guest physical address translation
 */

#ifndef _PUREVISOR_EPT_H
#define _PUREVISOR_EPT_H

#include <lib/types.h>
#include <vmm/vmx.h>

/* ============================================================================
 * EPT Constants
 * ============================================================================ */

#define EPT_LEVELS          4
#define EPT_ENTRIES         512

/* EPT page sizes */
#define EPT_PAGE_SIZE_4K    (4 * KB)
#define EPT_PAGE_SIZE_2M    (2 * MB)
#define EPT_PAGE_SIZE_1G    (1 * GB)

/* EPT index extraction */
#define EPT_PML4_INDEX(addr)    (((addr) >> 39) & 0x1FF)
#define EPT_PDPT_INDEX(addr)    (((addr) >> 30) & 0x1FF)
#define EPT_PD_INDEX(addr)      (((addr) >> 21) & 0x1FF)
#define EPT_PT_INDEX(addr)      (((addr) >> 12) & 0x1FF)

/* EPT permission flags */
#define EPT_PERM_NONE       0
#define EPT_PERM_READ       EPT_READ
#define EPT_PERM_WRITE      EPT_WRITE
#define EPT_PERM_EXEC       EPT_EXECUTE
#define EPT_PERM_RW         (EPT_READ | EPT_WRITE)
#define EPT_PERM_RX         (EPT_READ | EPT_EXECUTE)
#define EPT_PERM_RWX        (EPT_READ | EPT_WRITE | EPT_EXECUTE)

/* ============================================================================
 * EPT Entry Types
 * ============================================================================ */

typedef uint64_t ept_entry_t;

/* ============================================================================
 * EPT Context
 * ============================================================================ */

typedef struct {
    ept_entry_t *pml4;          /* PML4 table (virtual address) */
    phys_addr_t pml4_phys;      /* PML4 physical address */
    uint64_t eptp;              /* EPT Pointer (for VMCS) */
    
    /* Memory tracking */
    uint64_t mapped_pages;
    uint64_t total_memory;
} ept_context_t;

/* ============================================================================
 * EPT Violation Information
 * ============================================================================ */

typedef struct {
    phys_addr_t guest_phys;     /* Guest physical address */
    uint64_t guest_linear;      /* Guest linear address (if valid) */
    
    /* Access type that caused violation */
    bool read;
    bool write;
    bool execute;
    
    /* Page state */
    bool page_present;
    bool caused_by_translation;
    
    /* EPT entry at time of violation */
    ept_entry_t entry;
} ept_violation_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * ept_create - Create a new EPT context
 * 
 * Returns EPT context or NULL on failure
 */
ept_context_t *ept_create(void);

/**
 * ept_destroy - Destroy an EPT context
 * @ept: EPT context to destroy
 */
void ept_destroy(ept_context_t *ept);

/**
 * ept_map_page - Map a guest physical page to host physical page
 * @ept: EPT context
 * @guest_phys: Guest physical address
 * @host_phys: Host physical address
 * @perm: Permission flags (EPT_PERM_*)
 * @memtype: Memory type (EPT_MEMTYPE_*)
 * 
 * Returns 0 on success, -1 on failure
 */
int ept_map_page(ept_context_t *ept, phys_addr_t guest_phys, 
                 phys_addr_t host_phys, uint32_t perm, uint32_t memtype);

/**
 * ept_map_page_2m - Map a 2MB guest physical page
 * @ept: EPT context
 * @guest_phys: Guest physical address (2MB aligned)
 * @host_phys: Host physical address (2MB aligned)
 * @perm: Permission flags
 * @memtype: Memory type
 */
int ept_map_page_2m(ept_context_t *ept, phys_addr_t guest_phys,
                    phys_addr_t host_phys, uint32_t perm, uint32_t memtype);

/**
 * ept_map_page_1g - Map a 1GB guest physical page
 * @ept: EPT context
 * @guest_phys: Guest physical address (1GB aligned)
 * @host_phys: Host physical address (1GB aligned)
 * @perm: Permission flags
 * @memtype: Memory type
 */
int ept_map_page_1g(ept_context_t *ept, phys_addr_t guest_phys,
                    phys_addr_t host_phys, uint32_t perm, uint32_t memtype);

/**
 * ept_unmap_page - Unmap a guest physical page
 * @ept: EPT context
 * @guest_phys: Guest physical address
 */
int ept_unmap_page(ept_context_t *ept, phys_addr_t guest_phys);

/**
 * ept_map_range - Map a range of guest physical memory
 * @ept: EPT context
 * @guest_phys: Starting guest physical address
 * @host_phys: Starting host physical address
 * @size: Size of range
 * @perm: Permission flags
 * @memtype: Memory type
 */
int ept_map_range(ept_context_t *ept, phys_addr_t guest_phys,
                  phys_addr_t host_phys, uint64_t size,
                  uint32_t perm, uint32_t memtype);

/**
 * ept_set_permissions - Change page permissions
 * @ept: EPT context
 * @guest_phys: Guest physical address
 * @perm: New permission flags
 */
int ept_set_permissions(ept_context_t *ept, phys_addr_t guest_phys, 
                        uint32_t perm);

/**
 * ept_get_entry - Get EPT entry for guest physical address
 * @ept: EPT context
 * @guest_phys: Guest physical address
 * 
 * Returns EPT entry or 0 if not mapped
 */
ept_entry_t ept_get_entry(ept_context_t *ept, phys_addr_t guest_phys);

/**
 * ept_get_host_phys - Translate guest physical to host physical
 * @ept: EPT context
 * @guest_phys: Guest physical address
 * 
 * Returns host physical address or 0 if not mapped
 */
phys_addr_t ept_get_host_phys(ept_context_t *ept, phys_addr_t guest_phys);

/**
 * ept_handle_violation - Handle EPT violation
 * @ept: EPT context
 * @violation: Violation information
 * 
 * Returns 0 if handled, -1 if fatal
 */
int ept_handle_violation(ept_context_t *ept, ept_violation_t *violation);

/**
 * ept_invalidate - Invalidate EPT TLB entries
 * @ept: EPT context
 */
void ept_invalidate(ept_context_t *ept);

/**
 * ept_build_pointer - Build EPTP from EPT context
 * @ept: EPT context
 * 
 * Returns EPTP value for VMCS
 */
static inline uint64_t ept_build_pointer(ept_context_t *ept)
{
    return ept->pml4_phys | EPT_MEMTYPE_WB | EPT_PAGE_WALK_4;
}

#endif /* _PUREVISOR_EPT_H */
