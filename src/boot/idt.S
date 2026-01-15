/*
 * PureVisor - IDT Setup and Interrupt Handlers (GAS syntax)
 * 
 * Interrupt Descriptor Table and low-level interrupt handling
 */

.code64
.section .text

.global idt_load
.global isr_stub_table

/* External C handlers */
.extern exception_handler
.extern irq_handler

/* ===========================================================================
 * idt_load - Load the IDT
 * RDI = pointer to IDT pointer structure
 * =========================================================================== */
idt_load:
    lidt (%rdi)
    ret

/* ===========================================================================
 * Macro for ISR stub without error code
 * =========================================================================== */
.macro ISR_NOERR num
isr_stub_\num:
    pushq $0                        /* Dummy error code */
    pushq $\num                     /* Interrupt number */
    jmp isr_common
.endm

/* ===========================================================================
 * Macro for ISR stub with error code (pushed by CPU)
 * =========================================================================== */
.macro ISR_ERR num
isr_stub_\num:
    pushq $\num                     /* Interrupt number */
    jmp isr_common
.endm

/* ===========================================================================
 * Macro for IRQ stub
 * =========================================================================== */
.macro IRQ_STUB num, irq_num
irq_stub_\num:
    pushq $0                        /* Dummy error code */
    pushq $\irq_num                 /* IRQ number (remapped) */
    jmp irq_common
.endm

/* ===========================================================================
 * Exception Stubs (0-31)
 * =========================================================================== */
ISR_NOERR 0                         /* Divide Error */
ISR_NOERR 1                         /* Debug */
ISR_NOERR 2                         /* NMI */
ISR_NOERR 3                         /* Breakpoint */
ISR_NOERR 4                         /* Overflow */
ISR_NOERR 5                         /* Bound Range */
ISR_NOERR 6                         /* Invalid Opcode */
ISR_NOERR 7                         /* Device Not Available */
ISR_ERR   8                         /* Double Fault */
ISR_NOERR 9                         /* Coprocessor Segment Overrun */
ISR_ERR   10                        /* Invalid TSS */
ISR_ERR   11                        /* Segment Not Present */
ISR_ERR   12                        /* Stack Fault */
ISR_ERR   13                        /* General Protection */
ISR_ERR   14                        /* Page Fault */
ISR_NOERR 15                        /* Reserved */
ISR_NOERR 16                        /* x87 FPU Error */
ISR_ERR   17                        /* Alignment Check */
ISR_NOERR 18                        /* Machine Check */
ISR_NOERR 19                        /* SIMD FP Exception */
ISR_NOERR 20                        /* Virtualization Exception */
ISR_ERR   21                        /* Control Protection */
ISR_NOERR 22                        /* Reserved */
ISR_NOERR 23                        /* Reserved */
ISR_NOERR 24                        /* Reserved */
ISR_NOERR 25                        /* Reserved */
ISR_NOERR 26                        /* Reserved */
ISR_NOERR 27                        /* Reserved */
ISR_NOERR 28                        /* Hypervisor Injection */
ISR_ERR   29                        /* VMM Communication */
ISR_ERR   30                        /* Security Exception */
ISR_NOERR 31                        /* Reserved */

/* ===========================================================================
 * IRQ Stubs (32-47, remapped from 0-15)
 * =========================================================================== */
IRQ_STUB 0, 32                      /* Timer */
IRQ_STUB 1, 33                      /* Keyboard */
IRQ_STUB 2, 34                      /* Cascade */
IRQ_STUB 3, 35                      /* COM2 */
IRQ_STUB 4, 36                      /* COM1 */
IRQ_STUB 5, 37                      /* LPT2 */
IRQ_STUB 6, 38                      /* Floppy */
IRQ_STUB 7, 39                      /* LPT1 / Spurious */
IRQ_STUB 8, 40                      /* RTC */
IRQ_STUB 9, 41                      /* Free */
IRQ_STUB 10, 42                     /* Free */
IRQ_STUB 11, 43                     /* Free */
IRQ_STUB 12, 44                     /* Mouse */
IRQ_STUB 13, 45                     /* FPU */
IRQ_STUB 14, 46                     /* Primary ATA */
IRQ_STUB 15, 47                     /* Secondary ATA */

/* ===========================================================================
 * Common ISR Handler
 * Saves context and calls C exception handler
 * =========================================================================== */
isr_common:
    /* Save all general purpose registers */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    /* Save segment registers */
    movw %ds, %ax
    pushq %rax
    movw %es, %ax
    pushq %rax
    
    /* Load kernel data segment */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    
    /* Call C handler with pointer to interrupt frame */
    movq %rsp, %rdi                 /* First argument: pointer to registers */
    call exception_handler
    
    /* Restore segment registers */
    popq %rax
    movw %ax, %es
    popq %rax
    movw %ax, %ds
    
    /* Restore general purpose registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    /* Remove error code and interrupt number */
    addq $16, %rsp
    
    /* Return from interrupt */
    iretq

/* ===========================================================================
 * Common IRQ Handler
 * =========================================================================== */
irq_common:
    /* Save all general purpose registers */
    pushq %rax
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %rsi
    pushq %rdi
    pushq %rbp
    pushq %r8
    pushq %r9
    pushq %r10
    pushq %r11
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    
    /* Save segment registers */
    movw %ds, %ax
    pushq %rax
    movw %es, %ax
    pushq %rax
    
    /* Load kernel data segment */
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    
    /* Call C handler */
    movq %rsp, %rdi
    call irq_handler
    
    /* Restore segment registers */
    popq %rax
    movw %ax, %es
    popq %rax
    movw %ax, %ds
    
    /* Restore general purpose registers */
    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %r11
    popq %r10
    popq %r9
    popq %r8
    popq %rbp
    popq %rdi
    popq %rsi
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rax
    
    /* Remove error code and interrupt number */
    addq $16, %rsp
    
    iretq

/* ===========================================================================
 * ISR Stub Table
 * Array of pointers to ISR stubs for C code to use
 * =========================================================================== */
.section .data
.align 8

isr_stub_table:
    .quad isr_stub_0
    .quad isr_stub_1
    .quad isr_stub_2
    .quad isr_stub_3
    .quad isr_stub_4
    .quad isr_stub_5
    .quad isr_stub_6
    .quad isr_stub_7
    .quad isr_stub_8
    .quad isr_stub_9
    .quad isr_stub_10
    .quad isr_stub_11
    .quad isr_stub_12
    .quad isr_stub_13
    .quad isr_stub_14
    .quad isr_stub_15
    .quad isr_stub_16
    .quad isr_stub_17
    .quad isr_stub_18
    .quad isr_stub_19
    .quad isr_stub_20
    .quad isr_stub_21
    .quad isr_stub_22
    .quad isr_stub_23
    .quad isr_stub_24
    .quad isr_stub_25
    .quad isr_stub_26
    .quad isr_stub_27
    .quad isr_stub_28
    .quad isr_stub_29
    .quad isr_stub_30
    .quad isr_stub_31
    /* IRQs */
    .quad irq_stub_0
    .quad irq_stub_1
    .quad irq_stub_2
    .quad irq_stub_3
    .quad irq_stub_4
    .quad irq_stub_5
    .quad irq_stub_6
    .quad irq_stub_7
    .quad irq_stub_8
    .quad irq_stub_9
    .quad irq_stub_10
    .quad irq_stub_11
    .quad irq_stub_12
    .quad irq_stub_13
    .quad irq_stub_14
    .quad irq_stub_15
