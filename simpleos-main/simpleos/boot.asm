; File: boot.asm
; Multiboot-compliant kernel entry point
; Assembled with: nasm -f elf32 boot.asm -o boot.o

[BITS 32]

; Multiboot header constants
MBALIGN     equ 1 << 0              ; Align modules on page boundaries
MEMINFO     equ 1 << 1              ; Request memory map
FLAGS       equ MBALIGN | MEMINFO   ; Multiboot flags
MAGIC       equ 0x1BADB002          ; Multiboot magic number
CHECKSUM    equ -(MAGIC + FLAGS)    ; Checksum

; Multiboot header - must be in first 8KB of kernel
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Reserve stack space
section .bss
align 16
stack_bottom:
    resb 32768                      ; 32 KB stack (increased for VMM)
stack_top:

; Kernel entry point
section .text
global _start
global kernel_end
_start:
    ; Setup stack
    mov esp, stack_top
    
    ; Save multiboot info before we clobber registers
    ; EAX = magic number, EBX = multiboot info pointer
    push ebx                        ; Multiboot info structure pointer
    push eax                        ; Multiboot magic number
    
    ; Call kernel main
    extern kmain
    call kmain
    
    ; Halt if kmain returns
    cli
.hang:
    hlt
    jmp .hang

; Mark end of kernel
section .kernel_end
kernel_end:

; === GDT Definition ===
global gdt_start
global gdt_end
global gdt_pointer
global gdt_code_offset
global gdt_data_offset

section .data
align 8
gdt_start:
    ; Null descriptor (required)
    dq 0

gdt_code:
    ; Code segment: base=0, limit=4GB, 32-bit, ring 0
    dw 0xFFFF                       ; Limit low
    dw 0x0000                       ; Base low
    db 0x00                         ; Base middle
    db 10011010b                    ; Access: Present, Ring 0, Code, Executable, Readable
    db 11001111b                    ; Flags: 4KB granularity, 32-bit | Limit high
    db 0x00                         ; Base high

gdt_data:
    ; Data segment: base=0, limit=4GB, 32-bit, ring 0
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                    ; Access: Present, Ring 0, Data, Writable
    db 11001111b
    db 0x00

gdt_user_code:
    ; User code segment: ring 3
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11111010b                    ; Access: Present, Ring 3, Code, Executable, Readable
    db 11001111b
    db 0x00

gdt_user_data:
    ; User data segment: ring 3
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 11110010b                    ; Access: Present, Ring 3, Data, Writable
    db 11001111b
    db 0x00

gdt_tss:
    ; TSS descriptor - will be filled in by kernel
    dw 0x0068                       ; Limit (sizeof TSS - 1)
    dw 0x0000                       ; Base low (filled by kernel)
    db 0x00                         ; Base middle
    db 10001001b                    ; Access: Present, Ring 0, TSS
    db 00000000b                    ; Flags
    db 0x00                         ; Base high

gdt_end:

gdt_pointer:
    dw gdt_end - gdt_start - 1      ; Limit
    dd gdt_start                    ; Base

gdt_code_offset: equ gdt_code - gdt_start
gdt_data_offset: equ gdt_data - gdt_start
