; File: boot64.asm
; Multiboot loader + 32->64 bit trampoline for the OS/Two 64-bit kernel.
;
; QEMU/GRUB load this as a 32-bit multiboot image. It verifies long
; mode support, builds identity-mapped 4-level page tables (first 4GB
; with 2MB pages - covers RAM and the BGA framebuffer at ~0xFD000000),
; enables PAE + EFER.LME, loads a 64-bit GDT, copies the embedded
; 64-bit kernel payload to 2MB and jumps to it in long mode.

[BITS 32]

MBALIGN  equ 1 << 0
MEMINFO  equ 1 << 1
FLAGS    equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002

PAYLOAD_DEST equ 0x200000       ; 64-bit kernel runs at 2MB

section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd -(MAGIC + FLAGS)

section .bss
align 4096
pml4:   resb 4096
pdpt:   resb 4096
pds:    resb 4096 * 4           ; 4 page directories = 4GB of 2MB pages
align 16
stack:  resb 16384
stack_top:

section .data
align 8
gdt64:
    dq 0                        ; null
    dq 0x00209A0000000000       ; 0x08: 64-bit code (L=1, present, exec)
    dq 0x0000920000000000       ; 0x10: data
gdt64_ptr:
    dw gdt64_ptr - gdt64 - 1
    dd gdt64

mb_info_save: dd 0

section .payload
align 16
payload_start:
    incbin "kernel64.bin"
payload_end:

section .text
global _start
_start:
    cli
    mov esp, stack_top
    mov [mb_info_save], ebx

    ; --- long mode supported? ---
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode
    mov eax, 0x80000001
    cpuid
    test edx, 1 << 29           ; LM bit
    jz .no_long_mode

    ; --- copy the 64-bit kernel payload to 2MB ---
    mov esi, payload_start
    mov edi, PAYLOAD_DEST
    mov ecx, payload_end - payload_start
    add ecx, 3
    shr ecx, 2
    rep movsd

    ; --- page tables: identity map 0..4GB with 2MB pages ---
    mov eax, pdpt
    or eax, 0x03                ; present | writable
    mov [pml4], eax
    mov dword [pml4 + 4], 0

    xor ecx, ecx
.pdpt_fill:
    mov eax, ecx
    shl eax, 12                 ; pds + i*4096
    add eax, pds
    or eax, 0x03
    mov [pdpt + ecx * 8], eax
    mov dword [pdpt + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 4
    jb .pdpt_fill

    xor ecx, ecx                ; 2MB page index 0..2047
.pd_fill:
    mov eax, ecx
    shl eax, 21                 ; phys = index * 2MB
    or eax, 0x83                ; present | writable | 2MB page
    mov [pds + ecx * 8], eax
    mov dword [pds + ecx * 8 + 4], 0
    inc ecx
    cmp ecx, 2048
    jb .pd_fill

    ; --- enable long mode ---
    mov eax, cr4
    or eax, 1 << 5              ; PAE
    mov cr4, eax

    mov eax, pml4
    mov cr3, eax

    mov ecx, 0xC0000080         ; EFER
    rdmsr
    or eax, 1 << 8              ; LME
    wrmsr

    mov eax, cr0
    or eax, 0x80000001          ; PG | PE
    mov cr0, eax

    lgdt [gdt64_ptr]
    jmp 0x08:long_mode_entry

.no_long_mode:
    ; Report on serial and halt: this CPU cannot run the 64-bit kernel
    mov dx, 0x3F8
    mov al, 'N'
    out dx, al
    mov al, 'O'
    out dx, al
    mov al, '6'
    out dx, al
    mov al, '4'
    out dx, al
.halt:
    hlt
    jmp .halt

[BITS 64]
long_mode_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax
    mov rsp, stack_top

    mov edi, [mb_info_save]     ; arg 1: multiboot info pointer
    mov rax, PAYLOAD_DEST
    call rax

.hang:
    hlt
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits
