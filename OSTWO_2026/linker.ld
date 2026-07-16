/* File: linker.ld
 * Linker script for multiboot kernel
 */

ENTRY(_start)

SECTIONS {
    /* Load kernel at 1MB (standard for multiboot) */
    . = 1M;

    /* Multiboot header must be in first 8KB */
    .multiboot : {
        *(.multiboot)
    }

    /* Text section (code) */
    .text : {
        *(.text)
    }

    /* Read-only data */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    }

    /* Initialized data */
    .data : {
        *(.data)
    }

    /* Uninitialized data (BSS) */
    .bss : {
        *(COMMON)
        *(.bss)
    }

    /* Kernel end marker */
    .kernel_end : {
        *(.kernel_end)
    }

    /* End of kernel - align to page boundary */
    . = ALIGN(4096);
    _kernel_end = .;
}
