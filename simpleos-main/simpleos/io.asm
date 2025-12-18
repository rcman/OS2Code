; File: io.asm
; Hardware port I/O and CPU control functions
; Assembled with: nasm -f elf32 io.asm -o io.o

[BITS 32]

section .text

; === Port I/O Functions ===

global outb
outb:
    mov edx, [esp + 4]              ; Port
    mov al, [esp + 8]               ; Data
    out dx, al
    ret

global inb
inb:
    mov edx, [esp + 4]              ; Port
    in al, dx
    movzx eax, al
    ret

global outw
outw:
    mov edx, [esp + 4]              ; Port
    mov ax, [esp + 8]               ; Data
    out dx, ax
    ret

global inw
inw:
    mov edx, [esp + 4]              ; Port
    in ax, dx
    movzx eax, ax
    ret

global outl
outl:
    mov edx, [esp + 4]              ; Port
    mov eax, [esp + 8]              ; Data
    out dx, eax
    ret

global inl
inl:
    mov edx, [esp + 4]              ; Port
    in eax, dx
    ret

; === IO Wait (for slow devices) ===
global io_wait
io_wait:
    out 0x80, al                    ; Write to unused port
    ret

; === GDT/IDT Loading ===

global gdt_flush
gdt_flush:
    mov eax, [esp + 4]              ; GDT pointer
    lgdt [eax]
    
    ; Reload segment registers
    mov ax, 0x10                    ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Far jump to reload CS
    jmp 0x08:.flush
.flush:
    ret

global idt_flush
idt_flush:
    mov eax, [esp + 4]              ; IDT pointer
    lidt [eax]
    ret

; === TSS Loading ===

global tss_flush
tss_flush:
    mov ax, 0x2B                    ; TSS selector (5th entry, index 5 * 8 = 0x28 | RPL 3)
    ltr ax
    ret

; === Interrupt Control ===

global enable_interrupts
enable_interrupts:
    sti
    ret

global disable_interrupts
disable_interrupts:
    cli
    ret

global halt_cpu
halt_cpu:
    hlt
    ret
