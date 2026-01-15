/*
 * PureVisor UEFI Boot Loader Linker Script
 * 
 * Creates PE32+ executable for UEFI
 */

OUTPUT_FORMAT(elf64-x86-64)
OUTPUT_ARCH(i386:x86-64)
ENTRY(efi_main)

SECTIONS
{
    . = 0;
    
    .text ALIGN(4K) : {
        *(.text .text.*)
    }
    
    . = ALIGN(4K);
    .rodata : {
        *(.rodata .rodata.*)
    }
    
    . = ALIGN(4K);
    .data : {
        *(.data .data.*)
        *(.bss .bss.*)
        *(COMMON)
    }
    
    . = ALIGN(4K);
    .reloc : {
        *(.reloc)
    }
    
    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
