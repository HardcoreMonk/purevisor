/*
 * PureVisor - Multiboot2 Header (GAS syntax)
 * 
 * Multiboot2 specification compliant header for GRUB2/QEMU boot
 */

.section .multiboot2, "a"
.align 8

/* Multiboot2 Header */
mb2_header_start:
    .long 0xE85250D6                /* Magic number */
    .long 0                         /* Architecture: i386 (32-bit protected mode) */
    .long mb2_header_end - mb2_header_start  /* Header length */
    .long -(0xE85250D6 + 0 + (mb2_header_end - mb2_header_start)) /* Checksum */

/* Information Request Tag */
.align 8
mb2_info_request_tag:
    .word 1                         /* Type: Information request */
    .word 0                         /* Flags */
    .long mb2_info_request_end - mb2_info_request_tag
    .long 4                         /* Request: Basic memory info */
    .long 6                         /* Request: Memory map */
    .long 8                         /* Request: Framebuffer info */
    .long 9                         /* Request: ELF symbols */
    .long 14                        /* Request: ACPI old RSDP */
    .long 15                        /* Request: ACPI new RSDP */
mb2_info_request_end:

/* Framebuffer Tag (optional) */
.align 8
mb2_framebuffer_tag:
    .word 5                         /* Type: Framebuffer */
    .word 0                         /* Flags */
    .long mb2_framebuffer_end - mb2_framebuffer_tag
    .long 0                         /* Width */
    .long 0                         /* Height */
    .long 0                         /* Depth */
mb2_framebuffer_end:

/* Entry Address Tag */
.align 8
mb2_entry_tag:
    .word 3                         /* Type: Entry address */
    .word 0                         /* Flags */
    .long mb2_entry_end - mb2_entry_tag
    .long _start                    /* Entry point address */
mb2_entry_end:

/* Module Alignment Tag */
.align 8
mb2_module_align_tag:
    .word 6                         /* Type: Module alignment */
    .word 0                         /* Flags */
    .long 8                         /* Size */

/* End Tag */
.align 8
mb2_end_tag:
    .word 0                         /* Type: End tag */
    .word 0                         /* Flags */
    .long 8                         /* Size */

mb2_header_end:
