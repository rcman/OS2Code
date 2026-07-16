/* Linker script for native 64-bit OS/Two applications.
 * Linked at 4GB - above the kernel's identity-mapped region, where the
 * kernel maps fresh 4KB user pages. */
OUTPUT_FORMAT(elf64-x86-64)
ENTRY(_start)

SECTIONS
{
    . = 0x100000000;

    .text : { *(.text*) }
    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss : { *(COMMON) *(.bss*) }
}
