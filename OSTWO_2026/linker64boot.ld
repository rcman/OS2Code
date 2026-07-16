/* Linker script for the 32-bit multiboot loader carrying the embedded
 * 64-bit kernel payload (ostwo64.bin). */
OUTPUT_FORMAT(elf32-i386)
ENTRY(_start)

SECTIONS
{
    . = 0x100000;

    .text : {
        *(.multiboot)
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
        *(.payload)
    }

    .bss : {
        *(COMMON)
        *(.bss*)
    }
}
