/* Linker script for OS/2 Clone kernel */

ENTRY(_start)

SECTIONS
{
    /* Kernel loads at 0x1000 */
    . = 0x1000;

    .text : {
        *(.text)
    }

    .rodata : {
        *(.rodata)
    }

    .data : {
        *(.data)
    }

    .bss : {
        *(.bss)
        *(COMMON)
    }
}
