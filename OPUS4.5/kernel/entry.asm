; Kernel entry point
; Sets up stack and calls C++ kernel_main

[BITS 32]
[GLOBAL _start]
[EXTERN kernel_main]

section .text
_start:
    ; Set up stack
    mov esp, stack_top

    ; Call C++ kernel
    call kernel_main

    ; If kernel returns, halt
    cli
.hang:
    hlt
    jmp .hang

section .bss
align 16
stack_bottom:
    resb 16384              ; 16KB stack
stack_top:
