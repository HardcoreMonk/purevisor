/*
 * PureVisor - x86_64 Architecture Header
 * 
 * CPU registers, instructions, and architecture-specific definitions
 */

#ifndef _PUREVISOR_ARCH_X86_64_H
#define _PUREVISOR_ARCH_X86_64_H

#include <lib/types.h>

/* ============================================================================
 * Control Registers
 * ============================================================================ */

/* CR0 bits */
#define CR0_PE          BIT(0)      /* Protected Mode Enable */
#define CR0_MP          BIT(1)      /* Monitor Coprocessor */
#define CR0_EM          BIT(2)      /* Emulation */
#define CR0_TS          BIT(3)      /* Task Switched */
#define CR0_ET          BIT(4)      /* Extension Type */
#define CR0_NE          BIT(5)      /* Numeric Error */
#define CR0_WP          BIT(16)     /* Write Protect */
#define CR0_AM          BIT(18)     /* Alignment Mask */
#define CR0_NW          BIT(29)     /* Not Write-through */
#define CR0_CD          BIT(30)     /* Cache Disable */
#define CR0_PG          BIT(31)     /* Paging */

/* CR4 bits */
#define CR4_VME         BIT(0)      /* Virtual-8086 Mode Extensions */
#define CR4_PVI         BIT(1)      /* Protected-Mode Virtual Interrupts */
#define CR4_TSD         BIT(2)      /* Time Stamp Disable */
#define CR4_DE          BIT(3)      /* Debugging Extensions */
#define CR4_PSE         BIT(4)      /* Page Size Extension */
#define CR4_PAE         BIT(5)      /* Physical Address Extension */
#define CR4_MCE         BIT(6)      /* Machine-Check Enable */
#define CR4_PGE         BIT(7)      /* Page Global Enable */
#define CR4_PCE         BIT(8)      /* Performance-Monitoring Counter Enable */
#define CR4_OSFXSR      BIT(9)      /* OS Support for FXSAVE/FXRSTOR */
#define CR4_OSXMMEXCPT  BIT(10)     /* OS Support for Unmasked SIMD FP Exceptions */
#define CR4_UMIP        BIT(11)     /* User-Mode Instruction Prevention */
#define CR4_VMXE        BIT(13)     /* VMX Enable */
#define CR4_SMXE        BIT(14)     /* SMX Enable */
#define CR4_FSGSBASE    BIT(16)     /* Enable RDFSBASE/RDGSBASE/WRFSBASE/WRGSBASE */
#define CR4_PCIDE       BIT(17)     /* PCID Enable */
#define CR4_OSXSAVE     BIT(18)     /* XSAVE and Processor Extended States Enable */
#define CR4_SMEP        BIT(20)     /* Supervisor Mode Execution Prevention */
#define CR4_SMAP        BIT(21)     /* Supervisor Mode Access Prevention */

/* ============================================================================
 * RFLAGS Register
 * ============================================================================ */

#define RFLAGS_CF       BIT(0)      /* Carry Flag */
#define RFLAGS_PF       BIT(2)      /* Parity Flag */
#define RFLAGS_AF       BIT(4)      /* Auxiliary Carry Flag */
#define RFLAGS_ZF       BIT(6)      /* Zero Flag */
#define RFLAGS_SF       BIT(7)      /* Sign Flag */
#define RFLAGS_TF       BIT(8)      /* Trap Flag */
#define RFLAGS_IF       BIT(9)      /* Interrupt Enable Flag */
#define RFLAGS_DF       BIT(10)     /* Direction Flag */
#define RFLAGS_OF       BIT(11)     /* Overflow Flag */
#define RFLAGS_IOPL     BITS(13,12) /* I/O Privilege Level */
#define RFLAGS_NT       BIT(14)     /* Nested Task */
#define RFLAGS_RF       BIT(16)     /* Resume Flag */
#define RFLAGS_VM       BIT(17)     /* Virtual-8086 Mode */
#define RFLAGS_AC       BIT(18)     /* Alignment Check */
#define RFLAGS_VIF      BIT(19)     /* Virtual Interrupt Flag */
#define RFLAGS_VIP      BIT(20)     /* Virtual Interrupt Pending */
#define RFLAGS_ID       BIT(21)     /* ID Flag */

/* ============================================================================
 * MSR Registers
 * ============================================================================ */

#define MSR_IA32_APIC_BASE          0x0000001B
#define MSR_IA32_FEATURE_CONTROL    0x0000003A
#define MSR_IA32_SYSENTER_CS        0x00000174
#define MSR_IA32_SYSENTER_ESP       0x00000175
#define MSR_IA32_SYSENTER_EIP       0x00000176
#define MSR_IA32_PAT                0x00000277
#define MSR_IA32_EFER               0xC0000080
#define MSR_IA32_STAR               0xC0000081
#define MSR_IA32_LSTAR              0xC0000082
#define MSR_IA32_CSTAR              0xC0000083
#define MSR_IA32_FMASK              0xC0000084
#define MSR_IA32_FS_BASE            0xC0000100
#define MSR_IA32_GS_BASE            0xC0000101
#define MSR_IA32_KERNEL_GS_BASE     0xC0000102

/* VMX MSRs */
#define MSR_IA32_VMX_BASIC              0x00000480
#define MSR_IA32_VMX_PINBASED_CTLS      0x00000481
#define MSR_IA32_VMX_PROCBASED_CTLS     0x00000482
#define MSR_IA32_VMX_EXIT_CTLS          0x00000483
#define MSR_IA32_VMX_ENTRY_CTLS         0x00000484
#define MSR_IA32_VMX_MISC               0x00000485
#define MSR_IA32_VMX_CR0_FIXED0         0x00000486
#define MSR_IA32_VMX_CR0_FIXED1         0x00000487
#define MSR_IA32_VMX_CR4_FIXED0         0x00000488
#define MSR_IA32_VMX_CR4_FIXED1         0x00000489
#define MSR_IA32_VMX_PROCBASED_CTLS2    0x0000048B
#define MSR_IA32_VMX_EPT_VPID_CAP       0x0000048C
#define MSR_IA32_VMX_TRUE_PINBASED_CTLS 0x0000048D
#define MSR_IA32_VMX_TRUE_PROCBASED_CTLS 0x0000048E
#define MSR_IA32_VMX_TRUE_EXIT_CTLS     0x0000048F
#define MSR_IA32_VMX_TRUE_ENTRY_CTLS    0x00000490

/* EFER bits */
#define EFER_SCE        BIT(0)      /* System Call Extensions */
#define EFER_LME        BIT(8)      /* Long Mode Enable */
#define EFER_LMA        BIT(10)     /* Long Mode Active */
#define EFER_NXE        BIT(11)     /* No-Execute Enable */
#define EFER_SVME       BIT(12)     /* SVM Enable (AMD) */
#define EFER_LMSLE      BIT(13)     /* Long Mode Segment Limit Enable */
#define EFER_FFXSR      BIT(14)     /* Fast FXSAVE/FXRSTOR */
#define EFER_TCE        BIT(15)     /* Translation Cache Extension */

/* ============================================================================
 * CPUID
 * ============================================================================ */

/* CPUID Feature bits (ECX for leaf 1) */
#define CPUID_FEAT_ECX_VMX      BIT(5)
#define CPUID_FEAT_ECX_SMX      BIT(6)
#define CPUID_FEAT_ECX_XSAVE    BIT(26)
#define CPUID_FEAT_ECX_HYPERVISOR BIT(31)

/* CPUID Feature bits (EDX for leaf 1) */
#define CPUID_FEAT_EDX_MSR      BIT(5)
#define CPUID_FEAT_EDX_PAE      BIT(6)
#define CPUID_FEAT_EDX_APIC     BIT(9)
#define CPUID_FEAT_EDX_MTRR     BIT(12)
#define CPUID_FEAT_EDX_PGE      BIT(13)

/* AMD CPUID Feature bits (ECX for leaf 0x80000001) */
#define CPUID_AMD_FEAT_ECX_SVM  BIT(2)

/* ============================================================================
 * Inline Assembly - Register Access
 * ============================================================================ */

static ALWAYS_INLINE uint64_t read_cr0(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(val));
    return val;
}

static ALWAYS_INLINE void write_cr0(uint64_t val) {
    __asm__ __volatile__("mov %0, %%cr0" :: "r"(val));
}

static ALWAYS_INLINE uint64_t read_cr2(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(val));
    return val;
}

static ALWAYS_INLINE uint64_t read_cr3(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr3, %0" : "=r"(val));
    return val;
}

static ALWAYS_INLINE void write_cr3(uint64_t val) {
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(val));
}

static ALWAYS_INLINE uint64_t read_cr4(void) {
    uint64_t val;
    __asm__ __volatile__("mov %%cr4, %0" : "=r"(val));
    return val;
}

static ALWAYS_INLINE void write_cr4(uint64_t val) {
    __asm__ __volatile__("mov %0, %%cr4" :: "r"(val));
}

static ALWAYS_INLINE uint64_t read_rflags(void) {
    uint64_t val;
    __asm__ __volatile__("pushfq; popq %0" : "=r"(val));
    return val;
}

static ALWAYS_INLINE void write_rflags(uint64_t val) {
    __asm__ __volatile__("pushq %0; popfq" :: "r"(val) : "cc");
}

/* ============================================================================
 * Inline Assembly - MSR Access
 * ============================================================================ */

static ALWAYS_INLINE uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return ((uint64_t)hi << 32) | lo;
}

static ALWAYS_INLINE void wrmsr(uint32_t msr, uint64_t val) {
    uint32_t lo = val & 0xFFFFFFFF;
    uint32_t hi = val >> 32;
    __asm__ __volatile__("wrmsr" :: "a"(lo), "d"(hi), "c"(msr));
}

/* ============================================================================
 * Inline Assembly - CPUID
 * ============================================================================ */

typedef struct {
    uint32_t eax;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
} cpuid_result_t;

static ALWAYS_INLINE void cpuid(uint32_t leaf, uint32_t subleaf, cpuid_result_t *result) {
    __asm__ __volatile__(
        "cpuid"
        : "=a"(result->eax), "=b"(result->ebx), "=c"(result->ecx), "=d"(result->edx)
        : "a"(leaf), "c"(subleaf)
    );
}

/* ============================================================================
 * Inline Assembly - I/O Ports
 * ============================================================================ */

static ALWAYS_INLINE void outb(uint16_t port, uint8_t val) {
    __asm__ __volatile__("outb %0, %1" :: "a"(val), "Nd"(port));
}

static ALWAYS_INLINE void outw(uint16_t port, uint16_t val) {
    __asm__ __volatile__("outw %0, %1" :: "a"(val), "Nd"(port));
}

static ALWAYS_INLINE void outl(uint16_t port, uint32_t val) {
    __asm__ __volatile__("outl %0, %1" :: "a"(val), "Nd"(port));
}

static ALWAYS_INLINE uint8_t inb(uint16_t port) {
    uint8_t val;
    __asm__ __volatile__("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static ALWAYS_INLINE uint16_t inw(uint16_t port) {
    uint16_t val;
    __asm__ __volatile__("inw %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

static ALWAYS_INLINE uint32_t inl(uint16_t port) {
    uint32_t val;
    __asm__ __volatile__("inl %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}

/* ============================================================================
 * Inline Assembly - Miscellaneous
 * ============================================================================ */

static ALWAYS_INLINE void hlt(void) {
    __asm__ __volatile__("hlt");
}

static ALWAYS_INLINE void cli(void) {
    __asm__ __volatile__("cli");
}

static ALWAYS_INLINE void sti(void) {
    __asm__ __volatile__("sti");
}

static ALWAYS_INLINE void pause(void) {
    __asm__ __volatile__("pause");
}

static ALWAYS_INLINE void invlpg(void *addr) {
    __asm__ __volatile__("invlpg (%0)" :: "r"(addr) : "memory");
}

static ALWAYS_INLINE uint64_t rdtsc(void) {
    uint32_t lo, hi;
    __asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

/* ============================================================================
 * GDT / IDT Structures
 * ============================================================================ */

/* GDT Entry */
typedef struct PACKED {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
} gdt_entry_t;

/* GDT Entry (64-bit system segment) */
typedef struct PACKED {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_middle;
    uint8_t  access;
    uint8_t  flags_limit_high;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} gdt_entry64_t;

/* GDT Pointer */
typedef struct PACKED {
    uint16_t limit;
    uint64_t base;
} gdt_ptr_t;

/* IDT Entry (64-bit) */
typedef struct PACKED {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t  ist;           /* Interrupt Stack Table offset */
    uint8_t  type_attr;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
} idt_entry_t;

/* IDT Pointer */
typedef struct PACKED {
    uint16_t limit;
    uint64_t base;
} idt_ptr_t;

/* TSS (Task State Segment) */
typedef struct PACKED {
    uint32_t reserved0;
    uint64_t rsp0;
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist1;
    uint64_t ist2;
    uint64_t ist3;
    uint64_t ist4;
    uint64_t ist5;
    uint64_t ist6;
    uint64_t ist7;
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} tss_t;

/* GDT Access byte bits */
#define GDT_ACCESS_PRESENT      BIT(7)
#define GDT_ACCESS_DPL(x)       (((x) & 3) << 5)
#define GDT_ACCESS_SYSTEM       0
#define GDT_ACCESS_CODE_DATA    BIT(4)
#define GDT_ACCESS_EXECUTABLE   BIT(3)
#define GDT_ACCESS_DC           BIT(2)
#define GDT_ACCESS_RW           BIT(1)
#define GDT_ACCESS_ACCESSED     BIT(0)

/* GDT Flags nibble */
#define GDT_FLAG_GRANULARITY    BIT(3)
#define GDT_FLAG_SIZE           BIT(2)
#define GDT_FLAG_LONG           BIT(1)

/* IDT Type attributes */
#define IDT_ATTR_PRESENT        BIT(7)
#define IDT_ATTR_DPL(x)         (((x) & 3) << 5)
#define IDT_TYPE_INTERRUPT      0x0E
#define IDT_TYPE_TRAP           0x0F

/* Segment selectors */
#define GDT_NULL_SEL            0x00
#define GDT_KERNEL_CODE_SEL     0x08
#define GDT_KERNEL_DATA_SEL     0x10
#define GDT_USER_CODE_SEL       0x18
#define GDT_USER_DATA_SEL       0x20
#define GDT_TSS_SEL             0x28

/* ============================================================================
 * CPU Features
 * ============================================================================ */

typedef struct {
    bool vmx_supported;
    bool svm_supported;
    bool apic_present;
    bool x2apic_present;
    char vendor[13];
    char brand[49];
} cpu_features_t;

extern cpu_features_t cpu_features;

#endif /* _PUREVISOR_ARCH_X86_64_H */
