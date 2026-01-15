/*
 * PureVisor - VM Exit Handler Implementation
 * 
 * Handles VM exits and emulates guest operations
 */

#include <lib/types.h>
#include <lib/string.h>
#include <vmm/vmx.h>
#include <vmm/vcpu.h>
#include <vmm/ept.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * External Functions
 * ============================================================================ */

extern int vmcs_read(uint64_t field, uint64_t *value);
extern int vmcs_write(uint64_t field, uint64_t value);

/* ============================================================================
 * Exit Reason Names (for debugging)
 * ============================================================================ */

static const char *exit_reason_names[] = {
    [EXIT_REASON_EXCEPTION_NMI]     = "Exception/NMI",
    [EXIT_REASON_EXTERNAL_INT]      = "External Interrupt",
    [EXIT_REASON_TRIPLE_FAULT]      = "Triple Fault",
    [EXIT_REASON_INIT]              = "INIT Signal",
    [EXIT_REASON_SIPI]              = "SIPI",
    [EXIT_REASON_IO_SMI]            = "I/O SMI",
    [EXIT_REASON_OTHER_SMI]         = "Other SMI",
    [EXIT_REASON_INT_WINDOW]        = "Interrupt Window",
    [EXIT_REASON_NMI_WINDOW]        = "NMI Window",
    [EXIT_REASON_TASK_SWITCH]       = "Task Switch",
    [EXIT_REASON_CPUID]             = "CPUID",
    [EXIT_REASON_GETSEC]            = "GETSEC",
    [EXIT_REASON_HLT]               = "HLT",
    [EXIT_REASON_INVD]              = "INVD",
    [EXIT_REASON_INVLPG]            = "INVLPG",
    [EXIT_REASON_RDPMC]             = "RDPMC",
    [EXIT_REASON_RDTSC]             = "RDTSC",
    [EXIT_REASON_RSM]               = "RSM",
    [EXIT_REASON_VMCALL]            = "VMCALL",
    [EXIT_REASON_VMCLEAR]           = "VMCLEAR",
    [EXIT_REASON_VMLAUNCH]          = "VMLAUNCH",
    [EXIT_REASON_VMPTRLD]           = "VMPTRLD",
    [EXIT_REASON_VMPTRST]           = "VMPTRST",
    [EXIT_REASON_VMREAD]            = "VMREAD",
    [EXIT_REASON_VMRESUME]          = "VMRESUME",
    [EXIT_REASON_VMWRITE]           = "VMWRITE",
    [EXIT_REASON_VMXOFF]            = "VMXOFF",
    [EXIT_REASON_VMXON]             = "VMXON",
    [EXIT_REASON_CR_ACCESS]         = "CR Access",
    [EXIT_REASON_DR_ACCESS]         = "DR Access",
    [EXIT_REASON_IO_INSTR]          = "I/O Instruction",
    [EXIT_REASON_RDMSR]             = "RDMSR",
    [EXIT_REASON_WRMSR]             = "WRMSR",
    [EXIT_REASON_INVALID_GUEST_STATE] = "Invalid Guest State",
    [EXIT_REASON_MSR_LOADING]       = "MSR Loading",
    [EXIT_REASON_MWAIT]             = "MWAIT",
    [EXIT_REASON_MTF]               = "Monitor Trap Flag",
    [EXIT_REASON_MONITOR]           = "MONITOR",
    [EXIT_REASON_PAUSE]             = "PAUSE",
    [EXIT_REASON_MCE]               = "Machine Check",
    [EXIT_REASON_TPR_BELOW]         = "TPR Below Threshold",
    [EXIT_REASON_APIC_ACCESS]       = "APIC Access",
    [EXIT_REASON_VIRTUALIZED_EOI]   = "Virtualized EOI",
    [EXIT_REASON_GDTR_IDTR]         = "GDTR/IDTR Access",
    [EXIT_REASON_LDTR_TR]           = "LDTR/TR Access",
    [EXIT_REASON_EPT_VIOLATION]     = "EPT Violation",
    [EXIT_REASON_EPT_MISCONFIG]     = "EPT Misconfiguration",
    [EXIT_REASON_INVEPT]            = "INVEPT",
    [EXIT_REASON_RDTSCP]            = "RDTSCP",
    [EXIT_REASON_PREEMPTION_TIMER]  = "Preemption Timer",
    [EXIT_REASON_INVVPID]           = "INVVPID",
    [EXIT_REASON_WBINVD]            = "WBINVD",
    [EXIT_REASON_XSETBV]            = "XSETBV",
    [EXIT_REASON_APIC_WRITE]        = "APIC Write",
    [EXIT_REASON_RDRAND]            = "RDRAND",
    [EXIT_REASON_INVPCID]           = "INVPCID",
    [EXIT_REASON_VMFUNC]            = "VMFUNC",
    [EXIT_REASON_ENCLS]             = "ENCLS",
    [EXIT_REASON_RDSEED]            = "RDSEED",
    [EXIT_REASON_PML_FULL]          = "PML Full",
    [EXIT_REASON_XSAVES]            = "XSAVES",
    [EXIT_REASON_XRSTORS]           = "XRSTORS",
};

const char *vmexit_reason_str(uint32_t reason)
{
    reason &= 0xFFFF;  /* Remove flags */
    if (reason < sizeof(exit_reason_names) / sizeof(exit_reason_names[0])) {
        return exit_reason_names[reason] ? exit_reason_names[reason] : "Unknown";
    }
    return "Unknown";
}

/* ============================================================================
 * Instruction Advance Helper
 * ============================================================================ */

static void advance_guest_rip(vcpu_t *vcpu UNUSED)
{
    uint64_t rip, instr_len;
    vmcs_read(VMCS_GUEST_RIP, &rip);
    vmcs_read(VMCS_EXIT_INSTR_LENGTH, &instr_len);
    vmcs_write(VMCS_GUEST_RIP, rip + instr_len);
}

/* ============================================================================
 * CPUID Emulation
 * ============================================================================ */

static int handle_cpuid(vcpu_t *vcpu)
{
    uint32_t eax = (uint32_t)vcpu->regs.rax;
    uint32_t ecx = (uint32_t)vcpu->regs.rcx;
    cpuid_result_t result;
    
    /* Execute real CPUID */
    cpuid(eax, ecx, &result);
    
    /* Mask some features for the guest */
    if (eax == 1) {
        /* Hide VMX capability from guest */
        result.ecx &= ~CPUID_FEAT_ECX_VMX;
        /* Hide hypervisor bit */
        result.ecx &= ~(1 << 31);
    } else if (eax == 0x40000000) {
        /* Hypervisor signature leaf */
        result.eax = 0x40000001;  /* Max hypervisor leaf */
        result.ebx = 0x65727550;  /* "Pure" */
        result.ecx = 0x6F736956;  /* "Viso" */
        result.edx = 0x00000072;  /* "r\0\0\0" */
    } else if (eax == 0x40000001) {
        /* Hypervisor interface info */
        result.eax = 0;  /* No special interface */
        result.ebx = 0;
        result.ecx = 0;
        result.edx = 0;
    }
    
    vcpu->regs.rax = result.eax;
    vcpu->regs.rbx = result.ebx;
    vcpu->regs.rcx = result.ecx;
    vcpu->regs.rdx = result.edx;
    
    advance_guest_rip(vcpu);
    return 0;
}

/* ============================================================================
 * HLT Emulation
 * ============================================================================ */

static int handle_hlt(vcpu_t *vcpu)
{
    vcpu->state = VCPU_STATE_HALTED;
    advance_guest_rip(vcpu);
    
    /* In a real implementation, we would:
     * 1. Check for pending interrupts
     * 2. Yield CPU if no interrupts pending
     * 3. Resume when interrupt arrives
     */
    
    return 0;
}

/* ============================================================================
 * I/O Port Emulation
 * ============================================================================ */

static int handle_io(vcpu_t *vcpu)
{
    uint64_t qual = vcpu->exit_qualification;
    
    uint16_t port = (qual >> 16) & 0xFFFF;
    uint8_t size = (qual & 7) + 1;  /* 0=1byte, 1=2byte, 3=4byte */
    bool is_in = (qual >> 3) & 1;
    bool is_string = (qual >> 4) & 1;
    bool is_rep = (qual >> 5) & 1;
    
    (void)is_string;
    (void)is_rep;
    
    if (is_in) {
        /* IN instruction */
        uint32_t value = 0xFFFFFFFF;  /* Default: all ones */
        
        /* Emulate specific ports */
        switch (port) {
            case 0x3F8 ... 0x3FF:  /* COM1 */
            case 0x2F8 ... 0x2FF:  /* COM2 */
                /* Serial port - return 0 for now */
                value = 0;
                break;
            case 0x60:  /* Keyboard data */
                value = 0;
                break;
            case 0x64:  /* Keyboard status */
                value = 0;  /* No data available */
                break;
            default:
                /* Unknown port */
                break;
        }
        
        /* Store result in AL/AX/EAX */
        vcpu->regs.rax = (vcpu->regs.rax & ~((1ULL << (size * 8)) - 1)) | 
                         (value & ((1ULL << (size * 8)) - 1));
    } else {
        /* OUT instruction */
        uint32_t value UNUSED = vcpu->regs.rax & ((1ULL << (size * 8)) - 1);
        
        switch (port) {
            case 0x3F8 ... 0x3FF:  /* COM1 */
                /* Could forward to real serial port */
                if (port == 0x3F8 && size == 1) {
                    /* Data register - print character */
                    /* kprintf("%c", (char)value); */
                }
                break;
            case 0x80:  /* Debug port */
                /* Often used for delay or POST codes */
                break;
            default:
                break;
        }
    }
    
    advance_guest_rip(vcpu);
    return 0;
}

/* ============================================================================
 * MSR Emulation
 * ============================================================================ */

static int handle_rdmsr(vcpu_t *vcpu)
{
    uint32_t msr = (uint32_t)vcpu->regs.rcx;
    uint64_t value = 0;
    
    switch (msr) {
        case MSR_IA32_EFER:
            vmcs_read(VMCS_GUEST_IA32_EFER, &value);
            break;
        case MSR_IA32_APIC_BASE:
            value = 0xFEE00900;  /* Default APIC base, BSP, enabled */
            break;
        case MSR_IA32_FS_BASE:
            vmcs_read(VMCS_GUEST_FS_BASE, &value);
            break;
        case MSR_IA32_GS_BASE:
            vmcs_read(VMCS_GUEST_GS_BASE, &value);
            break;
        default:
            /* Unknown MSR - return 0 */
            pr_warn("VM Exit: RDMSR unknown MSR 0x%x", msr);
            value = 0;
            break;
    }
    
    vcpu->regs.rax = (uint32_t)value;
    vcpu->regs.rdx = (uint32_t)(value >> 32);
    
    advance_guest_rip(vcpu);
    return 0;
}

static int handle_wrmsr(vcpu_t *vcpu)
{
    uint32_t msr = (uint32_t)vcpu->regs.rcx;
    uint64_t value = ((uint64_t)vcpu->regs.rdx << 32) | (uint32_t)vcpu->regs.rax;
    
    switch (msr) {
        case MSR_IA32_EFER:
            vmcs_write(VMCS_GUEST_IA32_EFER, value);
            break;
        case MSR_IA32_FS_BASE:
            vmcs_write(VMCS_GUEST_FS_BASE, value);
            break;
        case MSR_IA32_GS_BASE:
            vmcs_write(VMCS_GUEST_GS_BASE, value);
            break;
        default:
            pr_warn("VM Exit: WRMSR unknown MSR 0x%x = 0x%llx", msr, value);
            break;
    }
    
    advance_guest_rip(vcpu);
    return 0;
}

/* ============================================================================
 * CR Access Emulation
 * ============================================================================ */

static int handle_cr_access(vcpu_t *vcpu)
{
    uint64_t qual = vcpu->exit_qualification;
    
    int cr_num = qual & 0xF;
    int access_type = (qual >> 4) & 0x3;
    int reg = (qual >> 8) & 0xF;
    
    uint64_t *gpr;
    switch (reg) {
        case 0:  gpr = &vcpu->regs.rax; break;
        case 1:  gpr = &vcpu->regs.rcx; break;
        case 2:  gpr = &vcpu->regs.rdx; break;
        case 3:  gpr = &vcpu->regs.rbx; break;
        case 4:  gpr = &vcpu->regs.rsp; break;
        case 5:  gpr = &vcpu->regs.rbp; break;
        case 6:  gpr = &vcpu->regs.rsi; break;
        case 7:  gpr = &vcpu->regs.rdi; break;
        case 8:  gpr = &vcpu->regs.r8;  break;
        case 9:  gpr = &vcpu->regs.r9;  break;
        case 10: gpr = &vcpu->regs.r10; break;
        case 11: gpr = &vcpu->regs.r11; break;
        case 12: gpr = &vcpu->regs.r12; break;
        case 13: gpr = &vcpu->regs.r13; break;
        case 14: gpr = &vcpu->regs.r14; break;
        case 15: gpr = &vcpu->regs.r15; break;
        default: gpr = &vcpu->regs.rax; break;
    }
    
    if (access_type == 0) {
        /* MOV to CR */
        uint64_t value = *gpr;
        
        switch (cr_num) {
            case 0:
                vmcs_write(VMCS_GUEST_CR0, value);
                vmcs_write(VMCS_CR0_READ_SHADOW, value);
                break;
            case 3:
                vmcs_write(VMCS_GUEST_CR3, value);
                break;
            case 4:
                vmcs_write(VMCS_GUEST_CR4, value);
                vmcs_write(VMCS_CR4_READ_SHADOW, value);
                break;
        }
    } else if (access_type == 1) {
        /* MOV from CR */
        uint64_t value = 0;
        
        switch (cr_num) {
            case 0:
                vmcs_read(VMCS_GUEST_CR0, &value);
                break;
            case 3:
                vmcs_read(VMCS_GUEST_CR3, &value);
                break;
            case 4:
                vmcs_read(VMCS_GUEST_CR4, &value);
                break;
        }
        
        *gpr = value;
    }
    
    advance_guest_rip(vcpu);
    return 0;
}

/* ============================================================================
 * EPT Violation Handler
 * ============================================================================ */

static int handle_ept_violation(vcpu_t *vcpu)
{
    uint64_t qual = vcpu->exit_qualification;
    phys_addr_t guest_phys = vcpu->guest_phys_addr;
    
    bool read = qual & 1;
    bool write = (qual >> 1) & 1;
    bool execute = (qual >> 2) & 1;
    
    pr_error("EPT Violation: GPA=0x%llx R=%d W=%d X=%d",
             guest_phys, read, write, execute);
    
    /* This would need proper handling:
     * - Demand paging
     * - MMIO emulation
     * - etc.
     */
    
    return -1;  /* Fatal for now */
}

/* ============================================================================
 * VMCALL (Hypercall) Handler
 * ============================================================================ */

static int handle_vmcall(vcpu_t *vcpu)
{
    uint64_t call_num = vcpu->regs.rax;
    uint64_t arg1 = vcpu->regs.rbx;
    uint64_t arg2 = vcpu->regs.rcx;
    uint64_t arg3 = vcpu->regs.rdx;
    
    (void)arg1;
    (void)arg2;
    (void)arg3;
    
    pr_info("VMCALL: num=%llu", call_num);
    
    /* Handle hypercalls */
    switch (call_num) {
        case 0:  /* Print string (debug) */
            /* arg1 = guest physical address of string */
            break;
        case 1:  /* Get hypervisor info */
            vcpu->regs.rax = 0x50555245;  /* "PURE" */
            vcpu->regs.rbx = 0x00010000;  /* Version 1.0 */
            break;
        default:
            vcpu->regs.rax = (uint64_t)-1;  /* Unknown call */
            break;
    }
    
    advance_guest_rip(vcpu);
    return 0;
}

/* ============================================================================
 * Main VM Exit Handler
 * ============================================================================ */

int vmexit_handler(vcpu_t *vcpu)
{
    uint32_t reason = vcpu->exit_reason & 0xFFFF;
    int ret = 0;
    
    switch (reason) {
        case EXIT_REASON_CPUID:
            ret = handle_cpuid(vcpu);
            break;
            
        case EXIT_REASON_HLT:
            ret = handle_hlt(vcpu);
            break;
            
        case EXIT_REASON_IO_INSTR:
            ret = handle_io(vcpu);
            break;
            
        case EXIT_REASON_RDMSR:
            ret = handle_rdmsr(vcpu);
            break;
            
        case EXIT_REASON_WRMSR:
            ret = handle_wrmsr(vcpu);
            break;
            
        case EXIT_REASON_CR_ACCESS:
            ret = handle_cr_access(vcpu);
            break;
            
        case EXIT_REASON_EPT_VIOLATION:
            ret = handle_ept_violation(vcpu);
            break;
            
        case EXIT_REASON_VMCALL:
            ret = handle_vmcall(vcpu);
            break;
            
        case EXIT_REASON_EXTERNAL_INT:
            /* External interrupt - just return, will be handled by host */
            break;
            
        case EXIT_REASON_TRIPLE_FAULT:
            pr_error("VM Exit: Triple fault!");
            ret = -1;
            break;
            
        default:
            pr_warn("VM Exit: Unhandled reason %u (%s)", 
                    reason, vmexit_reason_str(reason));
            ret = -1;
            break;
    }
    
    return ret;
}
