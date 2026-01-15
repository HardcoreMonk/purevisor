;; ===========================================================================
;; PureVisor - IDT Setup and Interrupt Handlers
;; 
;; Interrupt Descriptor Table and low-level interrupt handling
;; ===========================================================================

bits 64
section .text

global idt_load
global isr_stub_table

;; External C handlers
extern exception_handler
extern irq_handler

;; ===========================================================================
;; idt_load - Load the IDT
;; RDI = pointer to IDT pointer structure
;; ===========================================================================
idt_load:
    lidt [rdi]
    ret

;; ===========================================================================
;; Macro for ISR stub without error code
;; ===========================================================================
%macro ISR_NOERR 1
isr_stub_%1:
    push 0                          ; Dummy error code
    push %1                         ; Interrupt number
    jmp isr_common
%endmacro

;; ===========================================================================
;; Macro for ISR stub with error code (pushed by CPU)
;; ===========================================================================
%macro ISR_ERR 1
isr_stub_%1:
    push %1                         ; Interrupt number
    jmp isr_common
%endmacro

;; ===========================================================================
;; Macro for IRQ stub
;; ===========================================================================
%macro IRQ_STUB 2
irq_stub_%1:
    push 0                          ; Dummy error code
    push %2                         ; IRQ number (remapped)
    jmp irq_common
%endmacro

;; ===========================================================================
;; Exception Stubs (0-31)
;; ===========================================================================
ISR_NOERR 0                         ; Divide Error
ISR_NOERR 1                         ; Debug
ISR_NOERR 2                         ; NMI
ISR_NOERR 3                         ; Breakpoint
ISR_NOERR 4                         ; Overflow
ISR_NOERR 5                         ; Bound Range
ISR_NOERR 6                         ; Invalid Opcode
ISR_NOERR 7                         ; Device Not Available
ISR_ERR   8                         ; Double Fault
ISR_NOERR 9                         ; Coprocessor Segment Overrun
ISR_ERR   10                        ; Invalid TSS
ISR_ERR   11                        ; Segment Not Present
ISR_ERR   12                        ; Stack Fault
ISR_ERR   13                        ; General Protection
ISR_ERR   14                        ; Page Fault
ISR_NOERR 15                        ; Reserved
ISR_NOERR 16                        ; x87 FPU Error
ISR_ERR   17                        ; Alignment Check
ISR_NOERR 18                        ; Machine Check
ISR_NOERR 19                        ; SIMD FP Exception
ISR_NOERR 20                        ; Virtualization Exception
ISR_ERR   21                        ; Control Protection
ISR_NOERR 22                        ; Reserved
ISR_NOERR 23                        ; Reserved
ISR_NOERR 24                        ; Reserved
ISR_NOERR 25                        ; Reserved
ISR_NOERR 26                        ; Reserved
ISR_NOERR 27                        ; Reserved
ISR_NOERR 28                        ; Hypervisor Injection
ISR_ERR   29                        ; VMM Communication
ISR_ERR   30                        ; Security Exception
ISR_NOERR 31                        ; Reserved

;; ===========================================================================
;; IRQ Stubs (32-47, remapped from 0-15)
;; ===========================================================================
IRQ_STUB 0, 32                      ; Timer
IRQ_STUB 1, 33                      ; Keyboard
IRQ_STUB 2, 34                      ; Cascade
IRQ_STUB 3, 35                      ; COM2
IRQ_STUB 4, 36                      ; COM1
IRQ_STUB 5, 37                      ; LPT2
IRQ_STUB 6, 38                      ; Floppy
IRQ_STUB 7, 39                      ; LPT1 / Spurious
IRQ_STUB 8, 40                      ; RTC
IRQ_STUB 9, 41                      ; Free
IRQ_STUB 10, 42                     ; Free
IRQ_STUB 11, 43                     ; Free
IRQ_STUB 12, 44                     ; Mouse
IRQ_STUB 13, 45                     ; FPU
IRQ_STUB 14, 46                     ; Primary ATA
IRQ_STUB 15, 47                     ; Secondary ATA

;; ===========================================================================
;; Common ISR Handler
;; Saves context and calls C exception handler
;; ===========================================================================
isr_common:
    ;; Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ;; Save segment registers
    mov ax, ds
    push rax
    mov ax, es
    push rax
    
    ;; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    ;; Call C handler with pointer to interrupt frame
    mov rdi, rsp                    ; First argument: pointer to registers
    call exception_handler
    
    ;; Restore segment registers
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ;; Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ;; Remove error code and interrupt number
    add rsp, 16
    
    ;; Return from interrupt
    iretq

;; ===========================================================================
;; Common IRQ Handler
;; ===========================================================================
irq_common:
    ;; Save all general purpose registers
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    
    ;; Save segment registers
    mov ax, ds
    push rax
    mov ax, es
    push rax
    
    ;; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    
    ;; Call C handler
    mov rdi, rsp
    call irq_handler
    
    ;; Restore segment registers
    pop rax
    mov es, ax
    pop rax
    mov ds, ax
    
    ;; Restore general purpose registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    ;; Remove error code and interrupt number
    add rsp, 16
    
    iretq

;; ===========================================================================
;; ISR Stub Table
;; Array of pointers to ISR stubs for C code to use
;; ===========================================================================
section .data
align 8

isr_stub_table:
    dq isr_stub_0
    dq isr_stub_1
    dq isr_stub_2
    dq isr_stub_3
    dq isr_stub_4
    dq isr_stub_5
    dq isr_stub_6
    dq isr_stub_7
    dq isr_stub_8
    dq isr_stub_9
    dq isr_stub_10
    dq isr_stub_11
    dq isr_stub_12
    dq isr_stub_13
    dq isr_stub_14
    dq isr_stub_15
    dq isr_stub_16
    dq isr_stub_17
    dq isr_stub_18
    dq isr_stub_19
    dq isr_stub_20
    dq isr_stub_21
    dq isr_stub_22
    dq isr_stub_23
    dq isr_stub_24
    dq isr_stub_25
    dq isr_stub_26
    dq isr_stub_27
    dq isr_stub_28
    dq isr_stub_29
    dq isr_stub_30
    dq isr_stub_31
    ;; IRQs
    dq irq_stub_0
    dq irq_stub_1
    dq irq_stub_2
    dq irq_stub_3
    dq irq_stub_4
    dq irq_stub_5
    dq irq_stub_6
    dq irq_stub_7
    dq irq_stub_8
    dq irq_stub_9
    dq irq_stub_10
    dq irq_stub_11
    dq irq_stub_12
    dq irq_stub_13
    dq irq_stub_14
    dq irq_stub_15
