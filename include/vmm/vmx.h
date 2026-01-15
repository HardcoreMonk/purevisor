/*
 * PureVisor - VMX Definitions Header
 * 
 * Intel VT-x constants, structures, and VMCS field encodings
 */

#ifndef _PUREVISOR_VMX_H
#define _PUREVISOR_VMX_H

#include <lib/types.h>

/* ============================================================================
 * VMX Instruction Results
 * ============================================================================ */

#define VMX_OK              0
#define VMX_FAIL_INVALID    1
#define VMX_FAIL_VALID      2

/* ============================================================================
 * VMCS Field Encodings
 * ============================================================================ */

/* 16-bit Control Fields */
#define VMCS_VPID                       0x0000
#define VMCS_POSTED_INT_NOTIFY          0x0002
#define VMCS_EPTP_INDEX                 0x0004

/* 16-bit Guest State Fields */
#define VMCS_GUEST_ES_SEL               0x0800
#define VMCS_GUEST_CS_SEL               0x0802
#define VMCS_GUEST_SS_SEL               0x0804
#define VMCS_GUEST_DS_SEL               0x0806
#define VMCS_GUEST_FS_SEL               0x0808
#define VMCS_GUEST_GS_SEL               0x080A
#define VMCS_GUEST_LDTR_SEL             0x080C
#define VMCS_GUEST_TR_SEL               0x080E
#define VMCS_GUEST_INT_STATUS           0x0810
#define VMCS_GUEST_PML_INDEX            0x0812

/* 16-bit Host State Fields */
#define VMCS_HOST_ES_SEL                0x0C00
#define VMCS_HOST_CS_SEL                0x0C02
#define VMCS_HOST_SS_SEL                0x0C04
#define VMCS_HOST_DS_SEL                0x0C06
#define VMCS_HOST_FS_SEL                0x0C08
#define VMCS_HOST_GS_SEL                0x0C0A
#define VMCS_HOST_TR_SEL                0x0C0C

/* 64-bit Control Fields */
#define VMCS_IO_BITMAP_A                0x2000
#define VMCS_IO_BITMAP_B                0x2002
#define VMCS_MSR_BITMAP                 0x2004
#define VMCS_EXIT_MSR_STORE_ADDR        0x2006
#define VMCS_EXIT_MSR_LOAD_ADDR         0x2008
#define VMCS_ENTRY_MSR_LOAD_ADDR        0x200A
#define VMCS_EXECUTIVE_VMCS_PTR         0x200C
#define VMCS_PML_ADDR                   0x200E
#define VMCS_TSC_OFFSET                 0x2010
#define VMCS_VIRTUAL_APIC_ADDR          0x2012
#define VMCS_APIC_ACCESS_ADDR           0x2014
#define VMCS_POSTED_INT_DESC_ADDR       0x2016
#define VMCS_VMFUNC_CONTROLS            0x2018
#define VMCS_EPT_PTR                    0x201A
#define VMCS_EOI_EXIT_BITMAP_0          0x201C
#define VMCS_EOI_EXIT_BITMAP_1          0x201E
#define VMCS_EOI_EXIT_BITMAP_2          0x2020
#define VMCS_EOI_EXIT_BITMAP_3          0x2022
#define VMCS_EPTP_LIST_ADDR             0x2024
#define VMCS_VMREAD_BITMAP              0x2026
#define VMCS_VMWRITE_BITMAP             0x2028
#define VMCS_XSS_EXIT_BITMAP            0x202C
#define VMCS_ENCLS_EXIT_BITMAP          0x202E
#define VMCS_TSC_MULTIPLIER             0x2032

/* 64-bit Read-Only Data Fields */
#define VMCS_GUEST_PHYS_ADDR            0x2400

/* 64-bit Guest State Fields */
#define VMCS_VMCS_LINK_PTR              0x2800
#define VMCS_GUEST_IA32_DEBUGCTL        0x2802
#define VMCS_GUEST_IA32_PAT             0x2804
#define VMCS_GUEST_IA32_EFER            0x2806
#define VMCS_GUEST_IA32_PERF_GLOBAL_CTRL 0x2808
#define VMCS_GUEST_PDPTE0               0x280A
#define VMCS_GUEST_PDPTE1               0x280C
#define VMCS_GUEST_PDPTE2               0x280E
#define VMCS_GUEST_PDPTE3               0x2810
#define VMCS_GUEST_IA32_BNDCFGS         0x2812

/* 64-bit Host State Fields */
#define VMCS_HOST_IA32_PAT              0x2C00
#define VMCS_HOST_IA32_EFER             0x2C02
#define VMCS_HOST_IA32_PERF_GLOBAL_CTRL 0x2C04

/* 32-bit Control Fields */
#define VMCS_PIN_BASED_CONTROLS         0x4000
#define VMCS_PRIMARY_PROC_CONTROLS      0x4002
#define VMCS_EXCEPTION_BITMAP           0x4004
#define VMCS_PAGE_FAULT_ERROR_MASK      0x4006
#define VMCS_PAGE_FAULT_ERROR_MATCH     0x4008
#define VMCS_CR3_TARGET_COUNT           0x400A
#define VMCS_EXIT_CONTROLS              0x400C
#define VMCS_EXIT_MSR_STORE_COUNT       0x400E
#define VMCS_EXIT_MSR_LOAD_COUNT        0x4010
#define VMCS_ENTRY_CONTROLS             0x4012
#define VMCS_ENTRY_MSR_LOAD_COUNT       0x4014
#define VMCS_ENTRY_INT_INFO             0x4016
#define VMCS_ENTRY_EXCEPTION_ERROR      0x4018
#define VMCS_ENTRY_INSTR_LENGTH         0x401A
#define VMCS_TPR_THRESHOLD              0x401C
#define VMCS_SECONDARY_PROC_CONTROLS    0x401E
#define VMCS_PLE_GAP                    0x4020
#define VMCS_PLE_WINDOW                 0x4022

/* 32-bit Read-Only Data Fields */
#define VMCS_VM_INSTR_ERROR             0x4400
#define VMCS_EXIT_REASON                0x4402
#define VMCS_EXIT_INT_INFO              0x4404
#define VMCS_EXIT_INT_ERROR_CODE        0x4406
#define VMCS_IDT_VECTORING_INFO         0x4408
#define VMCS_IDT_VECTORING_ERROR_CODE   0x440A
#define VMCS_EXIT_INSTR_LENGTH          0x440C
#define VMCS_EXIT_INSTR_INFO            0x440E

/* 32-bit Guest State Fields */
#define VMCS_GUEST_ES_LIMIT             0x4800
#define VMCS_GUEST_CS_LIMIT             0x4802
#define VMCS_GUEST_SS_LIMIT             0x4804
#define VMCS_GUEST_DS_LIMIT             0x4806
#define VMCS_GUEST_FS_LIMIT             0x4808
#define VMCS_GUEST_GS_LIMIT             0x480A
#define VMCS_GUEST_LDTR_LIMIT           0x480C
#define VMCS_GUEST_TR_LIMIT             0x480E
#define VMCS_GUEST_GDTR_LIMIT           0x4810
#define VMCS_GUEST_IDTR_LIMIT           0x4812
#define VMCS_GUEST_ES_ACCESS            0x4814
#define VMCS_GUEST_CS_ACCESS            0x4816
#define VMCS_GUEST_SS_ACCESS            0x4818
#define VMCS_GUEST_DS_ACCESS            0x481A
#define VMCS_GUEST_FS_ACCESS            0x481C
#define VMCS_GUEST_GS_ACCESS            0x481E
#define VMCS_GUEST_LDTR_ACCESS          0x4820
#define VMCS_GUEST_TR_ACCESS            0x4822
#define VMCS_GUEST_INT_STATE            0x4824
#define VMCS_GUEST_ACTIVITY_STATE       0x4826
#define VMCS_GUEST_SMBASE               0x4828
#define VMCS_GUEST_IA32_SYSENTER_CS     0x482A
#define VMCS_GUEST_PREEMPTION_TIMER     0x482E

/* 32-bit Host State Fields */
#define VMCS_HOST_IA32_SYSENTER_CS      0x4C00

/* Natural-Width Control Fields */
#define VMCS_CR0_GUEST_HOST_MASK        0x6000
#define VMCS_CR4_GUEST_HOST_MASK        0x6002
#define VMCS_CR0_READ_SHADOW            0x6004
#define VMCS_CR4_READ_SHADOW            0x6006
#define VMCS_CR3_TARGET_0               0x6008
#define VMCS_CR3_TARGET_1               0x600A
#define VMCS_CR3_TARGET_2               0x600C
#define VMCS_CR3_TARGET_3               0x600E

/* Natural-Width Read-Only Data Fields */
#define VMCS_EXIT_QUALIFICATION         0x6400
#define VMCS_IO_RCX                     0x6402
#define VMCS_IO_RSI                     0x6404
#define VMCS_IO_RDI                     0x6406
#define VMCS_IO_RIP                     0x6408
#define VMCS_GUEST_LINEAR_ADDR          0x640A

/* Natural-Width Guest State Fields */
#define VMCS_GUEST_CR0                  0x6800
#define VMCS_GUEST_CR3                  0x6802
#define VMCS_GUEST_CR4                  0x6804
#define VMCS_GUEST_ES_BASE              0x6806
#define VMCS_GUEST_CS_BASE              0x6808
#define VMCS_GUEST_SS_BASE              0x680A
#define VMCS_GUEST_DS_BASE              0x680C
#define VMCS_GUEST_FS_BASE              0x680E
#define VMCS_GUEST_GS_BASE              0x6810
#define VMCS_GUEST_LDTR_BASE            0x6812
#define VMCS_GUEST_TR_BASE              0x6814
#define VMCS_GUEST_GDTR_BASE            0x6816
#define VMCS_GUEST_IDTR_BASE            0x6818
#define VMCS_GUEST_DR7                  0x681A
#define VMCS_GUEST_RSP                  0x681C
#define VMCS_GUEST_RIP                  0x681E
#define VMCS_GUEST_RFLAGS               0x6820
#define VMCS_GUEST_PENDING_DBG_EXCEPT   0x6822
#define VMCS_GUEST_IA32_SYSENTER_ESP    0x6824
#define VMCS_GUEST_IA32_SYSENTER_EIP    0x6826

/* Natural-Width Host State Fields */
#define VMCS_HOST_CR0                   0x6C00
#define VMCS_HOST_CR3                   0x6C02
#define VMCS_HOST_CR4                   0x6C04
#define VMCS_HOST_FS_BASE               0x6C06
#define VMCS_HOST_GS_BASE               0x6C08
#define VMCS_HOST_TR_BASE               0x6C0A
#define VMCS_HOST_GDTR_BASE             0x6C0C
#define VMCS_HOST_IDTR_BASE             0x6C0E
#define VMCS_HOST_IA32_SYSENTER_ESP     0x6C10
#define VMCS_HOST_IA32_SYSENTER_EIP     0x6C12
#define VMCS_HOST_RSP                   0x6C14
#define VMCS_HOST_RIP                   0x6C16

/* ============================================================================
 * VM-Exit Reasons
 * ============================================================================ */

#define EXIT_REASON_EXCEPTION_NMI       0
#define EXIT_REASON_EXTERNAL_INT        1
#define EXIT_REASON_TRIPLE_FAULT        2
#define EXIT_REASON_INIT                3
#define EXIT_REASON_SIPI                4
#define EXIT_REASON_IO_SMI              5
#define EXIT_REASON_OTHER_SMI           6
#define EXIT_REASON_INT_WINDOW          7
#define EXIT_REASON_NMI_WINDOW          8
#define EXIT_REASON_TASK_SWITCH         9
#define EXIT_REASON_CPUID               10
#define EXIT_REASON_GETSEC              11
#define EXIT_REASON_HLT                 12
#define EXIT_REASON_INVD                13
#define EXIT_REASON_INVLPG              14
#define EXIT_REASON_RDPMC               15
#define EXIT_REASON_RDTSC               16
#define EXIT_REASON_RSM                 17
#define EXIT_REASON_VMCALL              18
#define EXIT_REASON_VMCLEAR             19
#define EXIT_REASON_VMLAUNCH            20
#define EXIT_REASON_VMPTRLD             21
#define EXIT_REASON_VMPTRST             22
#define EXIT_REASON_VMREAD              23
#define EXIT_REASON_VMRESUME            24
#define EXIT_REASON_VMWRITE             25
#define EXIT_REASON_VMXOFF              26
#define EXIT_REASON_VMXON               27
#define EXIT_REASON_CR_ACCESS           28
#define EXIT_REASON_DR_ACCESS           29
#define EXIT_REASON_IO_INSTR            30
#define EXIT_REASON_RDMSR               31
#define EXIT_REASON_WRMSR               32
#define EXIT_REASON_INVALID_GUEST_STATE 33
#define EXIT_REASON_MSR_LOADING         34
#define EXIT_REASON_MWAIT               36
#define EXIT_REASON_MTF                 37
#define EXIT_REASON_MONITOR             39
#define EXIT_REASON_PAUSE               40
#define EXIT_REASON_MCE                 41
#define EXIT_REASON_TPR_BELOW           43
#define EXIT_REASON_APIC_ACCESS         44
#define EXIT_REASON_VIRTUALIZED_EOI     45
#define EXIT_REASON_GDTR_IDTR           46
#define EXIT_REASON_LDTR_TR             47
#define EXIT_REASON_EPT_VIOLATION       48
#define EXIT_REASON_EPT_MISCONFIG       49
#define EXIT_REASON_INVEPT              50
#define EXIT_REASON_RDTSCP              51
#define EXIT_REASON_PREEMPTION_TIMER    52
#define EXIT_REASON_INVVPID             53
#define EXIT_REASON_WBINVD              54
#define EXIT_REASON_XSETBV              55
#define EXIT_REASON_APIC_WRITE          56
#define EXIT_REASON_RDRAND              57
#define EXIT_REASON_INVPCID             58
#define EXIT_REASON_VMFUNC              59
#define EXIT_REASON_ENCLS               60
#define EXIT_REASON_RDSEED              61
#define EXIT_REASON_PML_FULL            62
#define EXIT_REASON_XSAVES              63
#define EXIT_REASON_XRSTORS             64

/* ============================================================================
 * Pin-Based VM-Execution Controls
 * ============================================================================ */

#define PIN_BASED_EXT_INT_EXIT          BIT(0)
#define PIN_BASED_NMI_EXIT              BIT(3)
#define PIN_BASED_VIRTUAL_NMI           BIT(5)
#define PIN_BASED_PREEMPTION_TIMER      BIT(6)
#define PIN_BASED_POSTED_INT            BIT(7)

/* ============================================================================
 * Primary Processor-Based VM-Execution Controls
 * ============================================================================ */

#define CPU_BASED_INT_WINDOW_EXIT       BIT(2)
#define CPU_BASED_TSC_OFFSET            BIT(3)
#define CPU_BASED_HLT_EXIT              BIT(7)
#define CPU_BASED_INVLPG_EXIT           BIT(9)
#define CPU_BASED_MWAIT_EXIT            BIT(10)
#define CPU_BASED_RDPMC_EXIT            BIT(11)
#define CPU_BASED_RDTSC_EXIT            BIT(12)
#define CPU_BASED_CR3_LOAD_EXIT         BIT(15)
#define CPU_BASED_CR3_STORE_EXIT        BIT(16)
#define CPU_BASED_CR8_LOAD_EXIT         BIT(19)
#define CPU_BASED_CR8_STORE_EXIT        BIT(20)
#define CPU_BASED_TPR_SHADOW            BIT(21)
#define CPU_BASED_NMI_WINDOW_EXIT       BIT(22)
#define CPU_BASED_MOV_DR_EXIT           BIT(23)
#define CPU_BASED_UNCOND_IO_EXIT        BIT(24)
#define CPU_BASED_IO_BITMAP             BIT(25)
#define CPU_BASED_MTF                   BIT(27)
#define CPU_BASED_MSR_BITMAP            BIT(28)
#define CPU_BASED_MONITOR_EXIT          BIT(29)
#define CPU_BASED_PAUSE_EXIT            BIT(30)
#define CPU_BASED_SECONDARY_CONTROLS    BIT(31)

/* ============================================================================
 * Secondary Processor-Based VM-Execution Controls
 * ============================================================================ */

#define CPU_BASED2_VIRT_APIC_ACCESS     BIT(0)
#define CPU_BASED2_EPT                  BIT(1)
#define CPU_BASED2_DESC_TABLE_EXIT      BIT(2)
#define CPU_BASED2_RDTSCP               BIT(3)
#define CPU_BASED2_VIRT_X2APIC          BIT(4)
#define CPU_BASED2_VPID                 BIT(5)
#define CPU_BASED2_WBINVD_EXIT          BIT(6)
#define CPU_BASED2_UNRESTRICTED_GUEST   BIT(7)
#define CPU_BASED2_APIC_REG_VIRT        BIT(8)
#define CPU_BASED2_VIRT_INT_DELIVERY    BIT(9)
#define CPU_BASED2_PAUSE_LOOP_EXIT      BIT(10)
#define CPU_BASED2_RDRAND_EXIT          BIT(11)
#define CPU_BASED2_INVPCID              BIT(12)
#define CPU_BASED2_VMFUNC               BIT(13)
#define CPU_BASED2_VMCS_SHADOW          BIT(14)
#define CPU_BASED2_ENCLS_EXIT           BIT(15)
#define CPU_BASED2_RDSEED_EXIT          BIT(16)
#define CPU_BASED2_PML                  BIT(17)
#define CPU_BASED2_EPT_VE               BIT(18)
#define CPU_BASED2_XSAVES_XRSTORS       BIT(20)
#define CPU_BASED2_TSC_SCALING          BIT(25)

/* ============================================================================
 * VM-Exit Controls
 * ============================================================================ */

#define EXIT_CTRL_SAVE_DBG              BIT(2)
#define EXIT_CTRL_HOST_ADDR_SPACE       BIT(9)
#define EXIT_CTRL_LOAD_IA32_PERF        BIT(12)
#define EXIT_CTRL_ACK_INT_ON_EXIT       BIT(15)
#define EXIT_CTRL_SAVE_IA32_PAT         BIT(18)
#define EXIT_CTRL_LOAD_IA32_PAT         BIT(19)
#define EXIT_CTRL_SAVE_IA32_EFER        BIT(20)
#define EXIT_CTRL_LOAD_IA32_EFER        BIT(21)
#define EXIT_CTRL_SAVE_PREEMPT_TIMER    BIT(22)
#define EXIT_CTRL_CLEAR_BNDCFGS         BIT(23)
#define EXIT_CTRL_CONCEAL_VMX_FROM_PT   BIT(24)

/* ============================================================================
 * VM-Entry Controls
 * ============================================================================ */

#define ENTRY_CTRL_LOAD_DBG             BIT(2)
#define ENTRY_CTRL_IA32E_MODE           BIT(9)
#define ENTRY_CTRL_SMM                  BIT(10)
#define ENTRY_CTRL_DEACT_DUAL_MON       BIT(11)
#define ENTRY_CTRL_LOAD_IA32_PERF       BIT(13)
#define ENTRY_CTRL_LOAD_IA32_PAT        BIT(14)
#define ENTRY_CTRL_LOAD_IA32_EFER       BIT(15)
#define ENTRY_CTRL_LOAD_BNDCFGS         BIT(16)
#define ENTRY_CTRL_CONCEAL_VMX_FROM_PT  BIT(17)

/* ============================================================================
 * EPT (Extended Page Tables) Definitions
 * ============================================================================ */

/* EPT pointer structure bits */
#define EPT_MEMTYPE_UC                  0
#define EPT_MEMTYPE_WC                  1
#define EPT_MEMTYPE_WT                  4
#define EPT_MEMTYPE_WP                  5
#define EPT_MEMTYPE_WB                  6

#define EPT_PAGE_WALK_4                 (3 << 3)
#define EPT_ACCESS_DIRTY                BIT(6)

/* EPT entry bits */
#define EPT_READ                        BIT(0)
#define EPT_WRITE                       BIT(1)
#define EPT_EXECUTE                     BIT(2)
#define EPT_MEMTYPE_SHIFT               3
#define EPT_IGNORE_PAT                  BIT(6)
#define EPT_LARGE_PAGE                  BIT(7)
#define EPT_ACCESSED                    BIT(8)
#define EPT_DIRTY                       BIT(9)
#define EPT_EXECUTE_USER                BIT(10)

#define EPT_ADDR_MASK                   0x000FFFFFFFFFF000ULL

/* ============================================================================
 * Segment Access Rights
 * ============================================================================ */

#define SEG_ACCESS_ACCESSED             BIT(0)
#define SEG_ACCESS_RW                   BIT(1)   /* Data: Writable, Code: Readable */
#define SEG_ACCESS_DC                   BIT(2)   /* Data: Expand-down, Code: Conforming */
#define SEG_ACCESS_CODE                 BIT(3)
#define SEG_ACCESS_S                    BIT(4)   /* Descriptor type (1=code/data) */
#define SEG_ACCESS_DPL_SHIFT            5
#define SEG_ACCESS_PRESENT              BIT(7)
#define SEG_ACCESS_AVL                  BIT(12)
#define SEG_ACCESS_L                    BIT(13)  /* 64-bit code segment */
#define SEG_ACCESS_DB                   BIT(14)  /* Default operation size */
#define SEG_ACCESS_G                    BIT(15)  /* Granularity */
#define SEG_ACCESS_UNUSABLE             BIT(16)

/* Common access rights */
#define SEG_ACCESS_KERNEL_CODE64        (SEG_ACCESS_PRESENT | SEG_ACCESS_S | \
                                         SEG_ACCESS_CODE | SEG_ACCESS_RW | \
                                         SEG_ACCESS_L | SEG_ACCESS_ACCESSED)

#define SEG_ACCESS_KERNEL_DATA          (SEG_ACCESS_PRESENT | SEG_ACCESS_S | \
                                         SEG_ACCESS_RW | SEG_ACCESS_ACCESSED)

#define SEG_ACCESS_TSS                  (SEG_ACCESS_PRESENT | SEG_ACCESS_ACCESSED | 0x9)

#endif /* _PUREVISOR_VMX_H */
