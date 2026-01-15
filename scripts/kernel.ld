/*
 * PureVisor Kernel Linker Script
 * 
 * Memory Layout for x86_64 Higher Half Kernel
 * Kernel loaded at physical 0x100000 (1MB)
 * Mapped to virtual 0xFFFF800000100000
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(_start)

/* Higher half kernel base */
KERNEL_VMA = 0xFFFF800000000000;
KERNEL_LMA = 0x100000;  /* 1 MB physical */

SECTIONS
{
    . = KERNEL_LMA;
    
    /* Multiboot2 header must be in first 32KB */
    .multiboot2 ALIGN(8) : {
        KEEP(*(.multiboot2))
    }
    
    /* Code section */
    . = ALIGN(4K);
    _kernel_start = .;
    
    .text ALIGN(4K) : AT(ADDR(.text)) {
        _text_start = .;
        *(.text.boot)       /* Boot code first */
        *(.text .text.*)
        _text_end = .;
    }
    
    /* Read-only data */
    . = ALIGN(4K);
    .rodata ALIGN(4K) : AT(ADDR(.rodata)) {
        _rodata_start = .;
        *(.rodata .rodata.*)
        _rodata_end = .;
    }
    
    /* Initialized data */
    . = ALIGN(4K);
    .data ALIGN(4K) : AT(ADDR(.data)) {
        _data_start = .;
        *(.data .data.*)
        _data_end = .;
    }
    
    /* Uninitialized data (BSS) */
    . = ALIGN(4K);
    .bss ALIGN(4K) : AT(ADDR(.bss)) {
        _bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        _bss_end = .;
    }
    
    . = ALIGN(4K);
    _kernel_end = .;
    
    /* Kernel stack (16KB per CPU, initially one) */
    . = ALIGN(16);
    _stack_bottom = .;
    . += 16K;
    _stack_top = .;
    
    /* Discard unnecessary sections */
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}

/* Export symbols for kernel use */
PROVIDE(_kernel_size = _kernel_end - _kernel_start);
PROVIDE(_bss_size = _bss_end - _bss_start);
