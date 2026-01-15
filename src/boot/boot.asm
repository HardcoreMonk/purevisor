;; ===========================================================================
;; PureVisor - Boot Code
;; 
;; Entry point from bootloader (32-bit protected mode)
;; Transitions to 64-bit long mode and calls kernel_main
;; ===========================================================================

bits 32
section .text.boot

;; External symbols
extern kernel_main
extern _bss_start
extern _bss_end
extern _stack_top

;; Global symbols
global _start
global gdt64_ptr
global pml4_table

;; ===========================================================================
;; Entry Point (32-bit Protected Mode)
;; ===========================================================================
_start:
    ;; Disable interrupts
    cli
    
    ;; Save multiboot2 info pointer (EBX) and magic (EAX)
    mov edi, eax                    ; Multiboot magic
    mov esi, ebx                    ; Multiboot info pointer
    
    ;; Set up stack
    mov esp, stack32_top
    
    ;; Clear BSS section
    mov ecx, _bss_end
    sub ecx, _bss_start
    xor al, al
    mov edi, _bss_start
    rep stosb
    
    ;; Restore multiboot info
    mov edi, [saved_mb_magic]
    mov esi, [saved_mb_info]
    
    ;; Check for long mode support
    call check_long_mode
    test eax, eax
    jz .no_long_mode
    
    ;; Set up paging (4-level page tables for long mode)
    call setup_paging
    
    ;; Enable PAE in CR4
    mov eax, cr4
    or eax, (1 << 5)                ; CR4.PAE
    mov cr4, eax
    
    ;; Load PML4 address into CR3
    mov eax, pml4_table
    mov cr3, eax
    
    ;; Enable long mode in EFER MSR
    mov ecx, 0xC0000080             ; IA32_EFER MSR
    rdmsr
    or eax, (1 << 8)                ; EFER.LME (Long Mode Enable)
    wrmsr
    
    ;; Enable paging (and protected mode, but already enabled)
    mov eax, cr0
    or eax, (1 << 31)               ; CR0.PG
    mov cr0, eax
    
    ;; Load 64-bit GDT
    lgdt [gdt64_ptr]
    
    ;; Far jump to 64-bit code segment
    jmp 0x08:long_mode_entry

.no_long_mode:
    ;; Display error and halt
    mov esi, error_no_long_mode
    call print_string32
    jmp halt32

;; ===========================================================================
;; Check Long Mode Support
;; Returns: EAX = 1 if supported, 0 if not
;; ===========================================================================
check_long_mode:
    ;; Check CPUID support first
    pushfd
    pop eax
    mov ecx, eax
    xor eax, (1 << 21)              ; Flip ID bit
    push eax
    popfd
    pushfd
    pop eax
    push ecx
    popfd
    xor eax, ecx
    jz .no_cpuid
    
    ;; Check for extended CPUID
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    
    ;; Check for long mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)             ; LM bit
    jz .no_long_mode
    
    mov eax, 1
    ret

.no_cpuid:
.no_long_mode:
    xor eax, eax
    ret

;; ===========================================================================
;; Setup Paging (Identity map first 4GB + higher half mapping)
;; ===========================================================================
setup_paging:
    ;; Clear page tables
    mov edi, pml4_table
    mov ecx, 4096 * 4               ; 4 tables * 4096 bytes
    xor eax, eax
    rep stosb
    
    ;; PML4[0] -> PDPT (identity map for lower half)
    mov eax, pdpt_table
    or eax, 0x03                    ; Present + Writable
    mov [pml4_table], eax
    
    ;; PML4[256] -> PDPT (higher half kernel: 0xFFFF800000000000)
    mov [pml4_table + 256 * 8], eax
    
    ;; PDPT[0] -> PDT
    mov eax, pd_table
    or eax, 0x03
    mov [pdpt_table], eax
    
    ;; Set up PDT with 2MB pages (identity map first 4GB)
    mov edi, pd_table
    mov eax, 0x83                   ; Present + Writable + Huge (2MB)
    mov ecx, 512
.fill_pd:
    mov [edi], eax
    add eax, 0x200000               ; Next 2MB
    add edi, 8
    loop .fill_pd
    
    ret

;; ===========================================================================
;; Print String (32-bit mode) - VGA text mode
;; ESI = string pointer
;; ===========================================================================
print_string32:
    mov edi, 0xB8000
    mov ah, 0x0F                    ; White on black
.loop:
    lodsb
    test al, al
    jz .done
    stosw
    jmp .loop
.done:
    ret

;; ===========================================================================
;; Halt (32-bit)
;; ===========================================================================
halt32:
    cli
    hlt
    jmp halt32

;; ===========================================================================
;; 64-bit Long Mode Entry
;; ===========================================================================
bits 64
section .text

long_mode_entry:
    ;; Reload segment registers with 64-bit data segment
    mov ax, 0x10                    ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ;; Set up 64-bit stack
    mov rsp, _stack_top
    
    ;; Clear direction flag
    cld
    
    ;; Zero the upper 32 bits of arguments (multiboot info was in ESI/EDI)
    mov edi, edi                    ; Zero-extend EDI to RDI
    mov esi, esi                    ; Zero-extend ESI to RSI
    
    ;; Call kernel_main(magic, mb_info)
    ;; RDI = multiboot magic
    ;; RSI = multiboot info pointer
    xor rdi, rdi
    mov edi, [saved_mb_magic]
    xor rsi, rsi
    mov esi, [saved_mb_info]
    
    call kernel_main
    
    ;; kernel_main should not return, but if it does...
halt64:
    cli
    hlt
    jmp halt64

;; ===========================================================================
;; Data Section
;; ===========================================================================
section .data

;; Saved multiboot info
saved_mb_magic: dd 0
saved_mb_info:  dd 0

;; Error messages
error_no_long_mode: db "ERROR: CPU does not support 64-bit long mode!", 0

;; ===========================================================================
;; 64-bit GDT
;; ===========================================================================
align 16
gdt64:
    ;; Null descriptor
    dq 0
    
    ;; Code segment (selector 0x08)
    dw 0xFFFF                       ; Limit (ignored in long mode)
    dw 0                            ; Base low
    db 0                            ; Base middle
    db 0x9A                         ; Access: Present, Ring 0, Code, Readable
    db 0xAF                         ; Flags: Long mode, Limit high
    db 0                            ; Base high
    
    ;; Data segment (selector 0x10)
    dw 0xFFFF                       ; Limit
    dw 0                            ; Base low
    db 0                            ; Base middle
    db 0x92                         ; Access: Present, Ring 0, Data, Writable
    db 0xCF                         ; Flags: 4KB granularity, 32-bit
    db 0                            ; Base high
gdt64_end:

gdt64_ptr:
    dw gdt64_end - gdt64 - 1        ; Limit
    dq gdt64                        ; Base

;; ===========================================================================
;; BSS Section - Page Tables
;; ===========================================================================
section .bss
align 4096

;; Page tables (4KB aligned)
pml4_table:  resb 4096
pdpt_table:  resb 4096
pd_table:    resb 4096
pt_table:    resb 4096

;; 32-bit bootstrap stack
align 16
stack32_bottom: resb 4096
stack32_top:
