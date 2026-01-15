/*
 * PureVisor - VCPU Implementation
 * 
 * Virtual CPU management and VMCS setup
 */

#include <lib/types.h>
#include <lib/string.h>
#include <vmm/vmx.h>
#include <vmm/vcpu.h>
#include <vmm/ept.h>
#include <mm/pmm.h>
#include <mm/heap.h>
#include <kernel/console.h>
#include <arch/x86_64/cpu.h>

/* ============================================================================
 * External VMX Functions
 * ============================================================================ */

extern int vmcs_init(vcpu_t *vcpu);
extern int vmcs_write(uint64_t field, uint64_t value);
extern int vmcs_read(uint64_t field, uint64_t *value);
extern uint32_t vmx_get_pin_based_controls(uint32_t requested);
extern uint32_t vmx_get_proc_based_controls(uint32_t requested);
extern uint32_t vmx_get_proc_based_controls2(uint32_t requested);
extern uint32_t vmx_get_exit_controls(uint32_t requested);
extern uint32_t vmx_get_entry_controls(uint32_t requested);
extern bool vmx_has_ept(void);
extern bool vmx_has_unrestricted_guest(void);

/* ============================================================================
 * VM Entry/Exit Assembly (defined in vmx_asm.S)
 * ============================================================================ */

extern void vmx_guest_entry(void);
extern int vmx_vmlaunch(vcpu_t *vcpu);
extern int vmx_vmresume(vcpu_t *vcpu);

/* ============================================================================
 * VCPU Creation and Destruction
 * ============================================================================ */

vcpu_t *vcpu_create(vm_t *vm, uint32_t vcpu_id)
{
    vcpu_t *vcpu = kmalloc(sizeof(vcpu_t), GFP_KERNEL | GFP_ZERO);
    if (!vcpu) {
        pr_error("VCPU: Failed to allocate VCPU structure");
        return NULL;
    }
    
    vcpu->vcpu_id = vcpu_id;
    vcpu->vm_id = vm ? vm->vm_id : 0;
    vcpu->vm = vm;
    vcpu->state = VCPU_STATE_CREATED;
    vcpu->physical_cpu = -1;
    vcpu->launched = false;
    
    /* Allocate VMXON region (4KB aligned) */
    vcpu->vmxon_phys = pmm_alloc_page();
    if (!vcpu->vmxon_phys) {
        pr_error("VCPU: Failed to allocate VMXON region");
        goto fail_vmxon;
    }
    vcpu->vmxon_region = phys_to_virt(vcpu->vmxon_phys);
    memset(vcpu->vmxon_region, 0, PAGE_SIZE);
    
    /* Allocate VMCS region (4KB aligned) */
    vcpu->vmcs_phys = pmm_alloc_page();
    if (!vcpu->vmcs_phys) {
        pr_error("VCPU: Failed to allocate VMCS region");
        goto fail_vmcs;
    }
    vcpu->vmcs_region = phys_to_virt(vcpu->vmcs_phys);
    memset(vcpu->vmcs_region, 0, PAGE_SIZE);
    
    /* Allocate I/O bitmaps */
    vcpu->io_bitmap_a_phys = pmm_alloc_page();
    vcpu->io_bitmap_b_phys = pmm_alloc_page();
    if (!vcpu->io_bitmap_a_phys || !vcpu->io_bitmap_b_phys) {
        pr_error("VCPU: Failed to allocate I/O bitmaps");
        goto fail_io;
    }
    vcpu->io_bitmap_a = phys_to_virt(vcpu->io_bitmap_a_phys);
    vcpu->io_bitmap_b = phys_to_virt(vcpu->io_bitmap_b_phys);
    /* Set all bits = trap all I/O */
    memset(vcpu->io_bitmap_a, 0xFF, PAGE_SIZE);
    memset(vcpu->io_bitmap_b, 0xFF, PAGE_SIZE);
    
    /* Allocate MSR bitmap */
    vcpu->msr_bitmap_phys = pmm_alloc_page();
    if (!vcpu->msr_bitmap_phys) {
        pr_error("VCPU: Failed to allocate MSR bitmap");
        goto fail_msr;
    }
    vcpu->msr_bitmap = phys_to_virt(vcpu->msr_bitmap_phys);
    /* Set all bits = trap all MSRs */
    memset(vcpu->msr_bitmap, 0xFF, PAGE_SIZE);
    
    pr_info("VCPU: Created VCPU %u for VM %u", vcpu_id, vcpu->vm_id);
    return vcpu;
    
fail_msr:
    if (vcpu->io_bitmap_b_phys) pmm_free_page(vcpu->io_bitmap_b_phys);
fail_io:
    if (vcpu->io_bitmap_a_phys) pmm_free_page(vcpu->io_bitmap_a_phys);
    pmm_free_page(vcpu->vmcs_phys);
fail_vmcs:
    pmm_free_page(vcpu->vmxon_phys);
fail_vmxon:
    kfree(vcpu);
    return NULL;
}

void vcpu_destroy(vcpu_t *vcpu)
{
    if (!vcpu) return;
    
    pr_info("VCPU: Destroying VCPU %u", vcpu->vcpu_id);
    
    if (vcpu->msr_bitmap_phys) pmm_free_page(vcpu->msr_bitmap_phys);
    if (vcpu->io_bitmap_b_phys) pmm_free_page(vcpu->io_bitmap_b_phys);
    if (vcpu->io_bitmap_a_phys) pmm_free_page(vcpu->io_bitmap_a_phys);
    if (vcpu->vmcs_phys) pmm_free_page(vcpu->vmcs_phys);
    if (vcpu->vmxon_phys) pmm_free_page(vcpu->vmxon_phys);
    
    kfree(vcpu);
}

/* ============================================================================
 * VMCS Setup
 * ============================================================================ */

/* Setup host state in VMCS */
static int setup_host_state(vcpu_t *vcpu UNUSED)
{
    uint64_t cr0, cr3, cr4;
    uint16_t sel;
    
    /* Control registers */
    cr0 = read_cr0();
    cr3 = read_cr3();
    cr4 = read_cr4();
    
    vmcs_write(VMCS_HOST_CR0, cr0);
    vmcs_write(VMCS_HOST_CR3, cr3);
    vmcs_write(VMCS_HOST_CR4, cr4);
    
    /* Segment selectors */
    __asm__ __volatile__("mov %%cs, %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_CS_SEL, sel);
    __asm__ __volatile__("mov %%ss, %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_SS_SEL, sel);
    __asm__ __volatile__("mov %%ds, %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_DS_SEL, sel);
    __asm__ __volatile__("mov %%es, %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_ES_SEL, sel);
    __asm__ __volatile__("mov %%fs, %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_FS_SEL, sel);
    __asm__ __volatile__("mov %%gs, %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_GS_SEL, sel);
    __asm__ __volatile__("str %0" : "=r"(sel));
    vmcs_write(VMCS_HOST_TR_SEL, sel);
    
    /* Base addresses */
    vmcs_write(VMCS_HOST_FS_BASE, rdmsr(MSR_IA32_FS_BASE));
    vmcs_write(VMCS_HOST_GS_BASE, rdmsr(MSR_IA32_GS_BASE));
    
    /* GDT and IDT */
    gdt_ptr_t gdtr;
    idt_ptr_t idtr;
    __asm__ __volatile__("sgdt %0" : "=m"(gdtr));
    __asm__ __volatile__("sidt %0" : "=m"(idtr));
    vmcs_write(VMCS_HOST_GDTR_BASE, gdtr.base);
    vmcs_write(VMCS_HOST_IDTR_BASE, idtr.base);
    
    /* TR base - need to extract from GDT */
    /* For simplicity, we'll set this to 0 for now */
    vmcs_write(VMCS_HOST_TR_BASE, 0);
    
    /* Sysenter MSRs */
    vmcs_write(VMCS_HOST_IA32_SYSENTER_CS, rdmsr(MSR_IA32_SYSENTER_CS));
    vmcs_write(VMCS_HOST_IA32_SYSENTER_ESP, rdmsr(MSR_IA32_SYSENTER_ESP));
    vmcs_write(VMCS_HOST_IA32_SYSENTER_EIP, rdmsr(MSR_IA32_SYSENTER_EIP));
    
    /* EFER */
    vmcs_write(VMCS_HOST_IA32_EFER, rdmsr(MSR_IA32_EFER));
    
    /* RSP and RIP will be set before vmlaunch */
    
    return 0;
}

/* Setup guest state in VMCS for real mode */
static int setup_guest_state_real_mode(vcpu_t *vcpu UNUSED)
{
    /* Control registers - real mode defaults */
    uint64_t cr0 = CR0_PE | CR0_NE | CR0_ET;  /* Protected mode (required by VMX) */
    vmcs_write(VMCS_GUEST_CR0, cr0);
    vmcs_write(VMCS_GUEST_CR3, 0);
    vmcs_write(VMCS_GUEST_CR4, CR4_VMXE);
    
    /* CR0/CR4 shadows for guest */
    vmcs_write(VMCS_CR0_READ_SHADOW, 0);  /* Guest sees real mode */
    vmcs_write(VMCS_CR4_READ_SHADOW, 0);
    vmcs_write(VMCS_CR0_GUEST_HOST_MASK, CR0_PE | CR0_PG);
    vmcs_write(VMCS_CR4_GUEST_HOST_MASK, CR4_VMXE);
    
    /* Debug register */
    vmcs_write(VMCS_GUEST_DR7, 0x400);
    
    /* RFLAGS */
    vmcs_write(VMCS_GUEST_RFLAGS, 0x2);  /* Reserved bit must be 1 */
    
    /* RIP - start at 0x7C00 (typical BIOS load address) */
    vmcs_write(VMCS_GUEST_RIP, 0x7C00);
    vmcs_write(VMCS_GUEST_RSP, 0x7000);
    
    /* Segment registers - real mode style but in protected mode format */
    /* Access rights: Present, S=1 (code/data), Type varies */
    uint32_t code_ar = SEG_ACCESS_PRESENT | SEG_ACCESS_S | SEG_ACCESS_CODE | 
                       SEG_ACCESS_RW | SEG_ACCESS_ACCESSED;
    uint32_t data_ar = SEG_ACCESS_PRESENT | SEG_ACCESS_S | SEG_ACCESS_RW | 
                       SEG_ACCESS_ACCESSED;
    
    /* CS */
    vmcs_write(VMCS_GUEST_CS_SEL, 0);
    vmcs_write(VMCS_GUEST_CS_BASE, 0);
    vmcs_write(VMCS_GUEST_CS_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_CS_ACCESS, code_ar);
    
    /* DS, ES, FS, GS, SS */
    vmcs_write(VMCS_GUEST_DS_SEL, 0);
    vmcs_write(VMCS_GUEST_DS_BASE, 0);
    vmcs_write(VMCS_GUEST_DS_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_DS_ACCESS, data_ar);
    
    vmcs_write(VMCS_GUEST_ES_SEL, 0);
    vmcs_write(VMCS_GUEST_ES_BASE, 0);
    vmcs_write(VMCS_GUEST_ES_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_ES_ACCESS, data_ar);
    
    vmcs_write(VMCS_GUEST_FS_SEL, 0);
    vmcs_write(VMCS_GUEST_FS_BASE, 0);
    vmcs_write(VMCS_GUEST_FS_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_FS_ACCESS, data_ar);
    
    vmcs_write(VMCS_GUEST_GS_SEL, 0);
    vmcs_write(VMCS_GUEST_GS_BASE, 0);
    vmcs_write(VMCS_GUEST_GS_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_GS_ACCESS, data_ar);
    
    vmcs_write(VMCS_GUEST_SS_SEL, 0);
    vmcs_write(VMCS_GUEST_SS_BASE, 0);
    vmcs_write(VMCS_GUEST_SS_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_SS_ACCESS, data_ar);
    
    /* LDTR - unusable */
    vmcs_write(VMCS_GUEST_LDTR_SEL, 0);
    vmcs_write(VMCS_GUEST_LDTR_BASE, 0);
    vmcs_write(VMCS_GUEST_LDTR_LIMIT, 0);
    vmcs_write(VMCS_GUEST_LDTR_ACCESS, SEG_ACCESS_UNUSABLE);
    
    /* TR - 32-bit TSS */
    vmcs_write(VMCS_GUEST_TR_SEL, 0);
    vmcs_write(VMCS_GUEST_TR_BASE, 0);
    vmcs_write(VMCS_GUEST_TR_LIMIT, 0xFF);
    vmcs_write(VMCS_GUEST_TR_ACCESS, SEG_ACCESS_TSS);
    
    /* GDTR and IDTR */
    vmcs_write(VMCS_GUEST_GDTR_BASE, 0);
    vmcs_write(VMCS_GUEST_GDTR_LIMIT, 0xFFFF);
    vmcs_write(VMCS_GUEST_IDTR_BASE, 0);
    vmcs_write(VMCS_GUEST_IDTR_LIMIT, 0xFFFF);
    
    /* Sysenter MSRs */
    vmcs_write(VMCS_GUEST_IA32_SYSENTER_CS, 0);
    vmcs_write(VMCS_GUEST_IA32_SYSENTER_ESP, 0);
    vmcs_write(VMCS_GUEST_IA32_SYSENTER_EIP, 0);
    
    /* EFER - no long mode */
    vmcs_write(VMCS_GUEST_IA32_EFER, 0);
    
    /* Guest interruptibility and activity state */
    vmcs_write(VMCS_GUEST_INT_STATE, 0);
    vmcs_write(VMCS_GUEST_ACTIVITY_STATE, 0);  /* Active */
    vmcs_write(VMCS_GUEST_PENDING_DBG_EXCEPT, 0);
    
    /* VMCS link pointer */
    vmcs_write(VMCS_VMCS_LINK_PTR, 0xFFFFFFFFFFFFFFFFULL);
    
    return 0;
}

/* Setup execution controls */
static int setup_execution_controls(vcpu_t *vcpu)
{
    uint32_t pin_based, proc_based, proc_based2;
    uint32_t exit_controls, entry_controls;
    
    /* Pin-based controls */
    pin_based = vmx_get_pin_based_controls(
        PIN_BASED_EXT_INT_EXIT | PIN_BASED_NMI_EXIT
    );
    vmcs_write(VMCS_PIN_BASED_CONTROLS, pin_based);
    
    /* Primary processor-based controls */
    proc_based = vmx_get_proc_based_controls(
        CPU_BASED_HLT_EXIT |
        CPU_BASED_IO_BITMAP |
        CPU_BASED_MSR_BITMAP |
        CPU_BASED_SECONDARY_CONTROLS
    );
    vmcs_write(VMCS_PRIMARY_PROC_CONTROLS, proc_based);
    
    /* Secondary processor-based controls */
    proc_based2 = 0;
    if (vmx_has_ept()) {
        proc_based2 |= CPU_BASED2_EPT;
    }
    if (vmx_has_unrestricted_guest()) {
        proc_based2 |= CPU_BASED2_UNRESTRICTED_GUEST;
    }
    proc_based2 = vmx_get_proc_based_controls2(proc_based2);
    vmcs_write(VMCS_SECONDARY_PROC_CONTROLS, proc_based2);
    
    /* Exit controls */
    exit_controls = vmx_get_exit_controls(
        EXIT_CTRL_HOST_ADDR_SPACE |  /* 64-bit host */
        EXIT_CTRL_SAVE_IA32_EFER |
        EXIT_CTRL_LOAD_IA32_EFER |
        EXIT_CTRL_ACK_INT_ON_EXIT
    );
    vmcs_write(VMCS_EXIT_CONTROLS, exit_controls);
    
    /* Entry controls */
    entry_controls = vmx_get_entry_controls(
        ENTRY_CTRL_LOAD_IA32_EFER
    );
    vmcs_write(VMCS_ENTRY_CONTROLS, entry_controls);
    
    /* Exception bitmap - trap nothing initially */
    vmcs_write(VMCS_EXCEPTION_BITMAP, 0);
    
    /* Page fault mask/match */
    vmcs_write(VMCS_PAGE_FAULT_ERROR_MASK, 0);
    vmcs_write(VMCS_PAGE_FAULT_ERROR_MATCH, 0);
    
    /* CR3 target count */
    vmcs_write(VMCS_CR3_TARGET_COUNT, 0);
    
    /* I/O and MSR bitmaps */
    vmcs_write(VMCS_IO_BITMAP_A, vcpu->io_bitmap_a_phys);
    vmcs_write(VMCS_IO_BITMAP_B, vcpu->io_bitmap_b_phys);
    vmcs_write(VMCS_MSR_BITMAP, vcpu->msr_bitmap_phys);
    
    /* EPT pointer (if EPT enabled) */
    if (vcpu->eptp) {
        vmcs_write(VMCS_EPT_PTR, vcpu->eptp);
    }
    
    return 0;
}

int vcpu_init_vmcs(vcpu_t *vcpu)
{
    pr_info("VCPU: Initializing VMCS for VCPU %u", vcpu->vcpu_id);
    
    /* Initialize VMCS structure */
    int ret = vmcs_init(vcpu);
    if (ret != 0) {
        return ret;
    }
    
    /* Setup host state */
    ret = setup_host_state(vcpu);
    if (ret != 0) {
        pr_error("VCPU: Failed to setup host state");
        return ret;
    }
    
    /* Setup guest state (real mode) */
    ret = setup_guest_state_real_mode(vcpu);
    if (ret != 0) {
        pr_error("VCPU: Failed to setup guest state");
        return ret;
    }
    
    /* Setup execution controls */
    ret = setup_execution_controls(vcpu);
    if (ret != 0) {
        pr_error("VCPU: Failed to setup execution controls");
        return ret;
    }
    
    pr_info("VCPU: VMCS initialized");
    return 0;
}

/* ============================================================================
 * VCPU Run
 * ============================================================================ */

int vcpu_run(vcpu_t *vcpu)
{
    int ret;
    
    vcpu->state = VCPU_STATE_RUNNING;
    
    if (!vcpu->launched) {
        /* First run - use VMLAUNCH */
        ret = vmx_vmlaunch(vcpu);
        if (ret == VMX_OK) {
            vcpu->launched = true;
        } else {
            vcpu->vmentry_failures++;
            uint64_t error;
            vmcs_read(VMCS_VM_INSTR_ERROR, &error);
            pr_error("VCPU: VMLAUNCH failed, error=%llu", error);
            return -1;
        }
    } else {
        /* Subsequent runs - use VMRESUME */
        ret = vmx_vmresume(vcpu);
        if (ret != VMX_OK) {
            uint64_t error;
            vmcs_read(VMCS_VM_INSTR_ERROR, &error);
            pr_error("VCPU: VMRESUME failed, error=%llu", error);
            return -1;
        }
    }
    
    /* Read exit reason */
    uint64_t exit_reason;
    vmcs_read(VMCS_EXIT_REASON, &exit_reason);
    vcpu->exit_reason = (uint32_t)exit_reason;
    
    vmcs_read(VMCS_EXIT_QUALIFICATION, &vcpu->exit_qualification);
    vmcs_read(VMCS_GUEST_LINEAR_ADDR, &vcpu->guest_linear_addr);
    vmcs_read(VMCS_GUEST_PHYS_ADDR, &vcpu->guest_phys_addr);
    
    vcpu->exit_count++;
    
    return vcpu->exit_reason;
}

void vcpu_get_regs(vcpu_t *vcpu, guest_regs_t *regs)
{
    *regs = vcpu->regs;
    
    /* Also read from VMCS */
    uint64_t val;
    vmcs_read(VMCS_GUEST_RIP, &val); regs->rip = val;
    vmcs_read(VMCS_GUEST_RSP, &val); regs->rsp = val;
    vmcs_read(VMCS_GUEST_RFLAGS, &val); regs->rflags = val;
}

void vcpu_set_regs(vcpu_t *vcpu, const guest_regs_t *regs)
{
    vcpu->regs = *regs;
    
    /* Also write to VMCS */
    vmcs_write(VMCS_GUEST_RIP, regs->rip);
    vmcs_write(VMCS_GUEST_RSP, regs->rsp);
    vmcs_write(VMCS_GUEST_RFLAGS, regs->rflags);
}
