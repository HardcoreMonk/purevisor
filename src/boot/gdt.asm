;; ===========================================================================
;; PureVisor - GDT Setup
;; 
;; Global Descriptor Table management for x86_64
;; ===========================================================================

bits 64
section .text

global gdt_init
global gdt_load
global tss_load

extern gdt_ptr
extern tss

;; ===========================================================================
;; gdt_load - Load the GDT
;; RDI = pointer to GDT pointer structure
;; ===========================================================================
gdt_load:
    lgdt [rdi]
    
    ;; Reload code segment with far return
    push 0x08                       ; Kernel code segment
    lea rax, [rel .reload_cs]
    push rax
    retfq

.reload_cs:
    ;; Reload data segments
    mov ax, 0x10                    ; Kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ret

;; ===========================================================================
;; tss_load - Load the TSS
;; RDI = TSS selector
;; ===========================================================================
tss_load:
    ltr di
    ret

;; ===========================================================================
;; gdt_init - Initialize GDT (called from C)
;; Sets up GDT with kernel/user segments and TSS
;; ===========================================================================
gdt_init:
    ;; This function is implemented in C (src/kernel/gdt.c)
    ;; This assembly stub is for low-level operations only
    ret
