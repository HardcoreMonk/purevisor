;; ===========================================================================
;; PureVisor - Multiboot2 Header
;; 
;; Multiboot2 specification compliant header for GRUB2/QEMU boot
;; ===========================================================================

section .multiboot2
align 8

;; Multiboot2 Header
mb2_header_start:
    dd 0xE85250D6                   ; Magic number
    dd 0                            ; Architecture: i386 (32-bit protected mode)
    dd mb2_header_end - mb2_header_start ; Header length
    dd -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start)) ; Checksum

;; --------------------------------------------------------------------------
;; Information Request Tag
;; --------------------------------------------------------------------------
align 8
mb2_info_request_tag:
    dw 1                            ; Type: Information request
    dw 0                            ; Flags
    dd mb2_info_request_end - mb2_info_request_tag
    dd 4                            ; Request: Basic memory info
    dd 6                            ; Request: Memory map
    dd 8                            ; Request: Framebuffer info
    dd 9                            ; Request: ELF symbols
    dd 14                           ; Request: ACPI old RSDP
    dd 15                           ; Request: ACPI new RSDP
mb2_info_request_end:

;; --------------------------------------------------------------------------
;; Framebuffer Tag (optional, for graphical mode)
;; --------------------------------------------------------------------------
align 8
mb2_framebuffer_tag:
    dw 5                            ; Type: Framebuffer
    dw 0                            ; Flags (not required)
    dd mb2_framebuffer_end - mb2_framebuffer_tag
    dd 0                            ; Width (0 = no preference)
    dd 0                            ; Height
    dd 0                            ; Depth
mb2_framebuffer_end:

;; --------------------------------------------------------------------------
;; Entry Address Tag (for ELF headers without entry)
;; --------------------------------------------------------------------------
align 8
mb2_entry_tag:
    dw 3                            ; Type: Entry address
    dw 0                            ; Flags
    dd mb2_entry_end - mb2_entry_tag
    dd _start                       ; Entry point address
mb2_entry_end:

;; --------------------------------------------------------------------------
;; Module Alignment Tag
;; --------------------------------------------------------------------------
align 8
mb2_module_align_tag:
    dw 6                            ; Type: Module alignment
    dw 0                            ; Flags
    dd 8                            ; Size
mb2_module_align_end:

;; --------------------------------------------------------------------------
;; End Tag
;; --------------------------------------------------------------------------
align 8
mb2_end_tag:
    dw 0                            ; Type: End tag
    dw 0                            ; Flags
    dd 8                            ; Size

mb2_header_end:
