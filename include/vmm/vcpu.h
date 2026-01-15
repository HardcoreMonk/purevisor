/*
 * PureVisor - Virtual CPU and VM Header
 * 
 * VCPU state and virtual machine management
 */

#ifndef _PUREVISOR_VCPU_H
#define _PUREVISOR_VCPU_H

#include <lib/types.h>
#include <vmm/vmx.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_VCPUS           256
#define VMCS_SIZE           4096
#define IO_BITMAP_SIZE      4096
#define MSR_BITMAP_SIZE     4096

/* VCPU states */
typedef enum {
    VCPU_STATE_CREATED,
    VCPU_STATE_RUNNING,
    VCPU_STATE_HALTED,
    VCPU_STATE_WAITING,
    VCPU_STATE_SHUTDOWN
} vcpu_state_t;

/* ============================================================================
 * Guest Register State
 * ============================================================================ */

typedef struct {
    /* General purpose registers (must match assembly layout) */
    uint64_t rax;
    uint64_t rbx;
    uint64_t rcx;
    uint64_t rdx;
    uint64_t rsi;
    uint64_t rdi;
    uint64_t rbp;
    uint64_t r8;
    uint64_t r9;
    uint64_t r10;
    uint64_t r11;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    
    /* Instruction pointer and flags */
    uint64_t rip;
    uint64_t rflags;
    uint64_t rsp;
    
    /* Control registers */
    uint64_t cr0;
    uint64_t cr2;
    uint64_t cr3;
    uint64_t cr4;
    
    /* Debug registers */
    uint64_t dr7;
    
    /* Segment selectors */
    uint16_t cs;
    uint16_t ds;
    uint16_t es;
    uint16_t fs;
    uint16_t gs;
    uint16_t ss;
    uint16_t tr;
    uint16_t ldtr;
} guest_regs_t;

/* ============================================================================
 * Segment Descriptor
 * ============================================================================ */

typedef struct {
    uint16_t selector;
    uint64_t base;
    uint32_t limit;
    uint32_t access_rights;
} segment_desc_t;

/* ============================================================================
 * VCPU Structure
 * ============================================================================ */

typedef struct vcpu {
    /* VCPU identification */
    uint32_t vcpu_id;
    uint32_t vm_id;
    vcpu_state_t state;
    
    /* VMX regions (4KB aligned) */
    void *vmxon_region;
    void *vmcs_region;
    phys_addr_t vmxon_phys;
    phys_addr_t vmcs_phys;
    
    /* Guest register state */
    guest_regs_t regs;
    
    /* Host state (saved on VM exit) */
    uint64_t host_rsp;
    uint64_t host_rip;
    
    /* Bitmaps */
    void *io_bitmap_a;
    void *io_bitmap_b;
    void *msr_bitmap;
    phys_addr_t io_bitmap_a_phys;
    phys_addr_t io_bitmap_b_phys;
    phys_addr_t msr_bitmap_phys;
    
    /* EPT */
    void *ept_pml4;
    phys_addr_t ept_pml4_phys;
    uint64_t eptp;
    
    /* Exit information */
    uint32_t exit_reason;
    uint64_t exit_qualification;
    uint64_t guest_linear_addr;
    uint64_t guest_phys_addr;
    
    /* Statistics */
    uint64_t exit_count;
    uint64_t vmentry_failures;
    
    /* Pointer to parent VM */
    struct vm *vm;
    
    /* Physical CPU affinity */
    int32_t physical_cpu;
    
    /* Launch status */
    bool launched;
    
    /* Padding for alignment */
    uint8_t reserved[64];
} vcpu_t;

/* ============================================================================
 * VM Structure
 * ============================================================================ */

typedef struct vm {
    /* VM identification */
    uint32_t vm_id;
    char name[64];
    
    /* VCPUs */
    vcpu_t *vcpus[MAX_VCPUS];
    uint32_t vcpu_count;
    
    /* Memory */
    phys_addr_t memory_base;
    uint64_t memory_size;
    
    /* EPT (shared by all VCPUs) */
    void *ept_pml4;
    phys_addr_t ept_pml4_phys;
    uint64_t eptp;
    
    /* State */
    bool running;
    
    /* Statistics */
    uint64_t total_exits;
} vm_t;

/* ============================================================================
 * VCPU API Functions
 * ============================================================================ */

/**
 * vcpu_create - Create a new VCPU
 * @vm: Parent VM
 * @vcpu_id: VCPU ID within the VM
 * 
 * Returns VCPU pointer or NULL on failure
 */
vcpu_t *vcpu_create(vm_t *vm, uint32_t vcpu_id);

/**
 * vcpu_destroy - Destroy a VCPU
 * @vcpu: VCPU to destroy
 */
void vcpu_destroy(vcpu_t *vcpu);

/**
 * vcpu_init_vmcs - Initialize VMCS for a VCPU
 * @vcpu: VCPU to initialize
 */
int vcpu_init_vmcs(vcpu_t *vcpu);

/**
 * vcpu_run - Run VCPU (enter guest mode)
 * @vcpu: VCPU to run
 * 
 * Returns exit reason
 */
int vcpu_run(vcpu_t *vcpu);

/**
 * vcpu_get_regs - Get guest register state
 * @vcpu: VCPU
 * @regs: Output register state
 */
void vcpu_get_regs(vcpu_t *vcpu, guest_regs_t *regs);

/**
 * vcpu_set_regs - Set guest register state
 * @vcpu: VCPU
 * @regs: New register state
 */
void vcpu_set_regs(vcpu_t *vcpu, const guest_regs_t *regs);

/* ============================================================================
 * VM API Functions
 * ============================================================================ */

/**
 * vm_create - Create a new virtual machine
 * @name: VM name
 * @memory_size: Guest memory size
 * 
 * Returns VM pointer or NULL on failure
 */
vm_t *vm_create(const char *name, uint64_t memory_size);

/**
 * vm_destroy - Destroy a virtual machine
 * @vm: VM to destroy
 */
void vm_destroy(vm_t *vm);

/**
 * vm_add_vcpu - Add a VCPU to VM
 * @vm: Target VM
 * 
 * Returns VCPU pointer or NULL
 */
vcpu_t *vm_add_vcpu(vm_t *vm);

/**
 * vm_run - Start all VCPUs
 * @vm: VM to start
 */
int vm_run(vm_t *vm);

/**
 * vm_stop - Stop all VCPUs
 * @vm: VM to stop
 */
void vm_stop(vm_t *vm);

#endif /* _PUREVISOR_VCPU_H */
