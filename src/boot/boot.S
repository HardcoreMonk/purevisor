/*
 * PureVisor - Boot Code (GAS syntax)
 * 
 * Entry point from bootloader (32-bit protected mode)
 * Transitions to 64-bit long mode and calls kernel_main
 */

.code32
.section .text.boot, "ax"

/* External symbols */
.extern kernel_main
.extern _bss_start
.extern _bss_end
.extern _stack_top

/* Global symbols */
.global _start
.global gdt64_ptr
.global pml4_table

/* ===========================================================================
 * Entry Point (32-bit Protected Mode)
 * =========================================================================== */
_start:
    /* Disable interrupts */
    cli
    
    /* Save multiboot2 info pointer (EBX) and magic (EAX) */
    movl %eax, saved_mb_magic
    movl %ebx, saved_mb_info
    
    /* Set up stack */
    movl $stack32_top, %esp
    
    /* Clear BSS section */
    movl $_bss_start, %edi
    movl $_bss_end, %ecx
    subl %edi, %ecx
    xorb %al, %al
    rep stosb
    
    /* Check for long mode support */
    call check_long_mode
    testl %eax, %eax
    jz no_long_mode
    
    /* Set up paging (4-level page tables for long mode) */
    call setup_paging
    
    /* Enable PAE in CR4 */
    movl %cr4, %eax
    orl $(1 << 5), %eax             /* CR4.PAE */
    movl %eax, %cr4
    
    /* Load PML4 address into CR3 */
    movl $pml4_table, %eax
    movl %eax, %cr3
    
    /* Enable long mode in EFER MSR */
    movl $0xC0000080, %ecx          /* IA32_EFER MSR */
    rdmsr
    orl $(1 << 8), %eax             /* EFER.LME (Long Mode Enable) */
    wrmsr
    
    /* Enable paging */
    movl %cr0, %eax
    orl $(1 << 31), %eax            /* CR0.PG */
    movl %eax, %cr0
    
    /* Load 64-bit GDT */
    lgdt gdt64_ptr
    
    /* Far jump to 64-bit code segment */
    ljmp $0x08, $long_mode_entry

no_long_mode:
    /* Display error and halt */
    movl $error_no_long_mode, %esi
    call print_string32
    jmp halt32

/* ===========================================================================
 * Check Long Mode Support
 * Returns: EAX = 1 if supported, 0 if not
 * =========================================================================== */
check_long_mode:
    /* Check CPUID support first */
    pushfl
    popl %eax
    movl %eax, %ecx
    xorl $(1 << 21), %eax           /* Flip ID bit */
    pushl %eax
    popfl
    pushfl
    popl %eax
    pushl %ecx
    popfl
    xorl %ecx, %eax
    jz no_cpuid
    
    /* Check for extended CPUID */
    movl $0x80000000, %eax
    cpuid
    cmpl $0x80000001, %eax
    jb no_long_mode_ret
    
    /* Check for long mode */
    movl $0x80000001, %eax
    cpuid
    testl $(1 << 29), %edx          /* LM bit */
    jz no_long_mode_ret
    
    movl $1, %eax
    ret

no_cpuid:
no_long_mode_ret:
    xorl %eax, %eax
    ret

/* ===========================================================================
 * Setup Paging (Identity map first 4GB + higher half mapping)
 * =========================================================================== */
setup_paging:
    /* Clear page tables */
    movl $pml4_table, %edi
    movl $(4096 * 4), %ecx          /* 4 tables * 4096 bytes */
    xorl %eax, %eax
    rep stosb
    
    /* PML4[0] -> PDPT (identity map for lower half) */
    movl $pdpt_table, %eax
    orl $0x03, %eax                 /* Present + Writable */
    movl %eax, pml4_table
    
    /* PML4[256] -> PDPT (higher half kernel: 0xFFFF800000000000) */
    movl %eax, pml4_table + 256 * 8
    
    /* PDPT[0] -> PDT */
    movl $pd_table, %eax
    orl $0x03, %eax
    movl %eax, pdpt_table
    
    /* Set up PDT with 2MB pages (identity map first 1GB) */
    movl $pd_table, %edi
    movl $0x83, %eax                /* Present + Writable + Huge (2MB) */
    movl $512, %ecx

fill_pd:
    movl %eax, (%edi)
    movl $0, 4(%edi)                /* High 32 bits = 0 */
    addl $0x200000, %eax            /* Next 2MB */
    addl $8, %edi
    loop fill_pd
    
    ret

/* ===========================================================================
 * Print String (32-bit mode) - VGA text mode
 * ESI = string pointer
 * =========================================================================== */
print_string32:
    movl $0xB8000, %edi
    movb $0x0F, %ah                 /* White on black */
print_loop:
    lodsb
    testb %al, %al
    jz print_done
    stosw
    jmp print_loop
print_done:
    ret

/* ===========================================================================
 * Halt (32-bit)
 * =========================================================================== */
halt32:
    cli
    hlt
    jmp halt32

/* ===========================================================================
 * 64-bit Long Mode Entry
 * =========================================================================== */
.code64
.section .text

long_mode_entry:
    /* Reload segment registers with 64-bit data segment */
    movw $0x10, %ax                 /* Data segment selector */
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    movw %ax, %ss
    
    /* Set up 64-bit stack */
    movabsq $_stack_top, %rsp
    
    /* Clear direction flag */
    cld
    
    /* Load multiboot info into arguments */
    xorq %rdi, %rdi
    movl saved_mb_magic, %edi       /* First arg: multiboot magic */
    xorq %rsi, %rsi
    movl saved_mb_info, %esi        /* Second arg: multiboot info pointer */
    
    /* Call kernel_main(magic, mb_info) */
    call kernel_main
    
    /* kernel_main should not return, but if it does... */
halt64:
    cli
    hlt
    jmp halt64

/* ===========================================================================
 * Data Section
 * =========================================================================== */
.section .data

/* Saved multiboot info */
saved_mb_magic: .long 0
saved_mb_info:  .long 0

/* Error messages */
error_no_long_mode: .asciz "ERROR: CPU does not support 64-bit long mode!"

/* ===========================================================================
 * 64-bit GDT
 * =========================================================================== */
.align 16
gdt64:
    /* Null descriptor */
    .quad 0
    
    /* Code segment (selector 0x08) */
    .word 0xFFFF                    /* Limit (ignored in long mode) */
    .word 0                         /* Base low */
    .byte 0                         /* Base middle */
    .byte 0x9A                      /* Access: Present, Ring 0, Code, Readable */
    .byte 0xAF                      /* Flags: Long mode, Limit high */
    .byte 0                         /* Base high */
    
    /* Data segment (selector 0x10) */
    .word 0xFFFF                    /* Limit */
    .word 0                         /* Base low */
    .byte 0                         /* Base middle */
    .byte 0x92                      /* Access: Present, Ring 0, Data, Writable */
    .byte 0xCF                      /* Flags: 4KB granularity, 32-bit */
    .byte 0                         /* Base high */
gdt64_end:

gdt64_ptr:
    .word gdt64_end - gdt64 - 1     /* Limit */
    .quad gdt64                     /* Base */

/* ===========================================================================
 * BSS Section - Page Tables
 * =========================================================================== */
.section .bss
.align 4096

/* Page tables (4KB aligned) */
pml4_table:  .space 4096
pdpt_table:  .space 4096
pd_table:    .space 4096
pt_table:    .space 4096

/* 32-bit bootstrap stack */
.align 16
stack32_bottom: .space 4096
stack32_top:
