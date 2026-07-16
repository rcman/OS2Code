; File: entry64.asm
; 64-bit kernel entry stub. Placed at the very start of the payload
; binary (see linker64.ld) - the loader jumps to the payload base with
; EDI holding the multiboot info pointer.

[BITS 64]

section .entry
global _start64
extern kmain64

_start64:
    mov rsp, stack64_top
    and rsp, -16
    call kmain64
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack64:
    resb 32768
stack64_top:
