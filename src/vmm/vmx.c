/*
 * PureVisor - VMX Core Implementation
 * 
 * Intel VT-x initialization and basic operations
 */

#include <lib/types.h>
#include <lib/string.h>
#include <vmm/vmx.h>
#include <vmm/vcpu.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <kernel/apic.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * VMX Capability MSRs
 * ============================================================================ */

typedef struct {
    uint32_t pin_based_allowed0;
    uint32_t pin_based_allowed1;
    uint32_t proc_based_allowed0;
    uint32_t proc_based_allowed1;
    uint32_t proc_based2_allowed0;
    uint32_t proc_based2_allowed1;
    uint32_t exit_allowed0;
    uint32_t exit_allowed1;
    uint32_t entry_allowed0;
    uint32_t entry_allowed1;
    uint32_t vmcs_revision;
    bool ept_supported;
    bool vpid_supported;
    bool unrestricted_guest;
} vmx_caps_t;

static vmx_caps_t vmx_caps;
static bool vmx_initialized = false;

/* ============================================================================
 * VMX Instructions (Inline Assembly)
 * ============================================================================ */

static inline int vmxon(phys_addr_t vmxon_phys)
{
    uint8_t error;
    
    __asm__ __volatile__(
        "vmxon %[vmxon_phys]\n\t"
        "setna %[error]\n\t"
        : [error] "=rm" (error)
        : [vmxon_phys] "m" (vmxon_phys)
        : "cc", "memory"
    );
    
    return error ? VMX_FAIL_INVALID : VMX_OK;
}

static inline void vmxoff(void)
{
    __asm__ __volatile__("vmxoff" ::: "cc", "memory");
}

static inline int vmclear(phys_addr_t vmcs_phys)
{
    uint8_t error;
    
    __asm__ __volatile__(
        "vmclear %[vmcs_phys]\n\t"
        "setna %[error]\n\t"
        : [error] "=rm" (error)
        : [vmcs_phys] "m" (vmcs_phys)
        : "cc", "memory"
    );
    
    return error ? VMX_FAIL_INVALID : VMX_OK;
}

static inline int vmptrld(phys_addr_t vmcs_phys)
{
    uint8_t error;
    
    __asm__ __volatile__(
        "vmptrld %[vmcs_phys]\n\t"
        "setna %[error]\n\t"
        : [error] "=rm" (error)
        : [vmcs_phys] "m" (vmcs_phys)
        : "cc", "memory"
    );
    
    return error ? VMX_FAIL_INVALID : VMX_OK;
}

static inline int vmread(uint64_t field, uint64_t *value)
{
    uint8_t error;
    
    __asm__ __volatile__(
        "vmread %[field], %[value]\n\t"
        "setna %[error]\n\t"
        : [value] "=rm" (*value), [error] "=rm" (error)
        : [field] "r" (field)
        : "cc"
    );
    
    return error ? VMX_FAIL_INVALID : VMX_OK;
}

static inline int vmwrite(uint64_t field, uint64_t value)
{
    uint8_t error;
    
    __asm__ __volatile__(
        "vmwrite %[value], %[field]\n\t"
        "setna %[error]\n\t"
        : [error] "=rm" (error)
        : [field] "r" (field), [value] "rm" (value)
        : "cc"
    );
    
    return error ? VMX_FAIL_INVALID : VMX_OK;
}

/* ============================================================================
 * VMX Helper Functions
 * ============================================================================ */

/* Adjust control value based on allowed 0/1 bits */
static uint32_t adjust_controls(uint32_t value, uint32_t allowed0, uint32_t allowed1)
{
    value |= allowed0;      /* Set bits that must be 1 */
    value &= allowed1;      /* Clear bits that must be 0 */
    return value;
}

/* Read VMX capability MSR and extract allowed 0/1 values */
static void read_vmx_capability(uint32_t msr, uint32_t *allowed0, uint32_t *allowed1)
{
    uint64_t value = rdmsr(msr);
    *allowed0 = (uint32_t)value;
    *allowed1 = (uint32_t)(value >> 32);
}

/* Check if VMX is supported */
bool vmx_is_supported(void)
{
    cpuid_result_t result;
    
    /* Check CPUID.1:ECX.VMX */
    cpuid(1, 0, &result);
    if (!(result.ecx & CPUID_FEAT_ECX_VMX)) {
        return false;
    }
    
    /* Check IA32_FEATURE_CONTROL MSR */
    uint64_t feature_control = rdmsr(MSR_IA32_FEATURE_CONTROL);
    
    /* If locked and VMX disabled, cannot use VMX */
    if ((feature_control & 0x1) && !(feature_control & 0x4)) {
        return false;
    }
    
    return true;
}

/* ============================================================================
 * VMX Initialization
 * ============================================================================ */

int vmx_init(void)
{
    pr_info("VMX: Initializing Intel VT-x...");
    
    if (!vmx_is_supported()) {
        pr_error("VMX: Intel VT-x not supported or disabled");
        return -1;
    }
    
    /* Enable VMX in IA32_FEATURE_CONTROL if not locked */
    uint64_t feature_control = rdmsr(MSR_IA32_FEATURE_CONTROL);
    if (!(feature_control & 0x1)) {
        /* Not locked, enable VMX */
        feature_control |= 0x5;  /* Lock bit + Enable VMX outside SMX */
        wrmsr(MSR_IA32_FEATURE_CONTROL, feature_control);
    }
    
    /* Read VMX capabilities */
    uint64_t vmx_basic = rdmsr(MSR_IA32_VMX_BASIC);
    vmx_caps.vmcs_revision = (uint32_t)vmx_basic;
    
    pr_info("VMX: VMCS revision ID: 0x%x", vmx_caps.vmcs_revision);
    
    /* Read control capabilities */
    bool true_controls = (vmx_basic >> 55) & 1;
    
    if (true_controls) {
        read_vmx_capability(MSR_IA32_VMX_TRUE_PINBASED_CTLS,
                           &vmx_caps.pin_based_allowed0,
                           &vmx_caps.pin_based_allowed1);
        read_vmx_capability(MSR_IA32_VMX_TRUE_PROCBASED_CTLS,
                           &vmx_caps.proc_based_allowed0,
                           &vmx_caps.proc_based_allowed1);
        read_vmx_capability(MSR_IA32_VMX_TRUE_EXIT_CTLS,
                           &vmx_caps.exit_allowed0,
                           &vmx_caps.exit_allowed1);
        read_vmx_capability(MSR_IA32_VMX_TRUE_ENTRY_CTLS,
                           &vmx_caps.entry_allowed0,
                           &vmx_caps.entry_allowed1);
    } else {
        read_vmx_capability(MSR_IA32_VMX_PINBASED_CTLS,
                           &vmx_caps.pin_based_allowed0,
                           &vmx_caps.pin_based_allowed1);
        read_vmx_capability(MSR_IA32_VMX_PROCBASED_CTLS,
                           &vmx_caps.proc_based_allowed0,
                           &vmx_caps.proc_based_allowed1);
        read_vmx_capability(MSR_IA32_VMX_EXIT_CTLS,
                           &vmx_caps.exit_allowed0,
                           &vmx_caps.exit_allowed1);
        read_vmx_capability(MSR_IA32_VMX_ENTRY_CTLS,
                           &vmx_caps.entry_allowed0,
                           &vmx_caps.entry_allowed1);
    }
    
    /* Check for secondary controls */
    if (vmx_caps.proc_based_allowed1 & CPU_BASED_SECONDARY_CONTROLS) {
        read_vmx_capability(MSR_IA32_VMX_PROCBASED_CTLS2,
                           &vmx_caps.proc_based2_allowed0,
                           &vmx_caps.proc_based2_allowed1);
        
        vmx_caps.ept_supported = (vmx_caps.proc_based2_allowed1 & CPU_BASED2_EPT) != 0;
        vmx_caps.vpid_supported = (vmx_caps.proc_based2_allowed1 & CPU_BASED2_VPID) != 0;
        vmx_caps.unrestricted_guest = (vmx_caps.proc_based2_allowed1 & CPU_BASED2_UNRESTRICTED_GUEST) != 0;
    }
    
    pr_info("VMX: EPT=%s VPID=%s Unrestricted=%s",
            vmx_caps.ept_supported ? "yes" : "no",
            vmx_caps.vpid_supported ? "yes" : "no",
            vmx_caps.unrestricted_guest ? "yes" : "no");
    
    vmx_initialized = true;
    pr_info("VMX: Initialization complete");
    
    return 0;
}

/* ============================================================================
 * Per-CPU VMX Enable/Disable
 * ============================================================================ */

int vmx_enable_cpu(void *vmxon_region, phys_addr_t vmxon_phys)
{
    if (!vmx_initialized) {
        pr_error("VMX: Not initialized");
        return -1;
    }
    
    /* Set VMCS revision ID in VMXON region */
    *(uint32_t *)vmxon_region = vmx_caps.vmcs_revision;
    
    /* Set CR4.VMXE */
    uint64_t cr4 = read_cr4();
    cr4 |= CR4_VMXE;
    write_cr4(cr4);
    
    /* Execute VMXON */
    int ret = vmxon(vmxon_phys);
    if (ret != VMX_OK) {
        pr_error("VMX: VMXON failed");
        return -1;
    }
    
    pr_info("VMX: Enabled on CPU %u", lapic_get_id());
    return 0;
}

void vmx_disable_cpu(void)
{
    vmxoff();
    
    uint64_t cr4 = read_cr4();
    cr4 &= ~CR4_VMXE;
    write_cr4(cr4);
}

/* ============================================================================
 * VMCS Operations
 * ============================================================================ */

int vmcs_init(vcpu_t *vcpu)
{
    /* Set VMCS revision ID */
    *(uint32_t *)vcpu->vmcs_region = vmx_caps.vmcs_revision;
    
    /* Clear VMCS */
    int ret = vmclear(vcpu->vmcs_phys);
    if (ret != VMX_OK) {
        pr_error("VMX: VMCLEAR failed");
        return -1;
    }
    
    /* Load VMCS */
    ret = vmptrld(vcpu->vmcs_phys);
    if (ret != VMX_OK) {
        pr_error("VMX: VMPTRLD failed");
        return -1;
    }
    
    return 0;
}

int vmcs_write(uint64_t field, uint64_t value)
{
    return vmwrite(field, value);
}

int vmcs_read(uint64_t field, uint64_t *value)
{
    return vmread(field, value);
}

/* ============================================================================
 * VMCS Field Setup Helpers
 * ============================================================================ */

uint32_t vmx_get_pin_based_controls(uint32_t requested)
{
    return adjust_controls(requested,
                          vmx_caps.pin_based_allowed0,
                          vmx_caps.pin_based_allowed1);
}

uint32_t vmx_get_proc_based_controls(uint32_t requested)
{
    return adjust_controls(requested,
                          vmx_caps.proc_based_allowed0,
                          vmx_caps.proc_based_allowed1);
}

uint32_t vmx_get_proc_based_controls2(uint32_t requested)
{
    return adjust_controls(requested,
                          vmx_caps.proc_based2_allowed0,
                          vmx_caps.proc_based2_allowed1);
}

uint32_t vmx_get_exit_controls(uint32_t requested)
{
    return adjust_controls(requested,
                          vmx_caps.exit_allowed0,
                          vmx_caps.exit_allowed1);
}

uint32_t vmx_get_entry_controls(uint32_t requested)
{
    return adjust_controls(requested,
                          vmx_caps.entry_allowed0,
                          vmx_caps.entry_allowed1);
}

bool vmx_has_ept(void)
{
    return vmx_caps.ept_supported;
}

bool vmx_has_vpid(void)
{
    return vmx_caps.vpid_supported;
}

bool vmx_has_unrestricted_guest(void)
{
    return vmx_caps.unrestricted_guest;
}

/* ============================================================================
 * INVEPT and INVVPID
 * ============================================================================ */

void vmx_invept(uint64_t type, uint64_t eptp)
{
    struct {
        uint64_t eptp;
        uint64_t reserved;
    } descriptor = { eptp, 0 };
    
    __asm__ __volatile__(
        "invept %0, %1"
        :
        : "m" (descriptor), "r" (type)
        : "cc", "memory"
    );
}

void vmx_invvpid(uint64_t type, uint16_t vpid, uint64_t linear_addr)
{
    struct {
        uint16_t vpid;
        uint16_t reserved[3];
        uint64_t linear_addr;
    } descriptor = { vpid, {0, 0, 0}, linear_addr };
    
    __asm__ __volatile__(
        "invvpid %0, %1"
        :
        : "m" (descriptor), "r" (type)
        : "cc", "memory"
    );
}
