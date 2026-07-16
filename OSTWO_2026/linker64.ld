/* Linker script for the 64-bit kernel payload (kernel64.elf).
 * Converted to a flat binary with objcopy; the .entry section must be
 * first so the loader can jump to the payload base address. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start64)

SECTIONS
{
    . = 0x200000;

    .text : {
        *(.entry)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }
}
