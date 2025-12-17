; OS/2 Clone ISO Bootloader
; El Torito no-emulation mode bootloader
; Uses INT 13h extensions for CD-ROM reading

[BITS 16]
[ORG 0x7C00]

KERNEL_OFFSET equ 0x1000    ; Memory offset where we load the kernel

start:
    ; Set up segment registers
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00          ; Stack grows downward from bootloader

    ; Save boot drive (BIOS passes CD drive number in DL)
    mov [BOOT_DRIVE], dl

    ; Clear screen
    mov ax, 0x0003
    int 0x10

    ; Print welcome message
    mov si, MSG_BOOT
    call print_string

    ; Load kernel (copy from loaded image to expected address)
    mov si, MSG_LOAD
    call print_string
    call load_kernel_ext

    ; Switch to protected mode
    mov si, MSG_PMODE
    call print_string

    cli                     ; Disable interrupts
    lgdt [gdt_descriptor]   ; Load GDT

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Far jump to flush pipeline and enter 32-bit code
    jmp CODE_SEG:protected_mode_start

; ============================================
; 16-bit functions
; ============================================

print_string:
    pusha
.loop:
    lodsb
    cmp al, 0
    je .done
    mov ah, 0x0E
    mov bh, 0
    int 0x10
    jmp .loop
.done:
    popa
    ret

load_kernel_ext:
    pusha
    
    ; The BIOS has already loaded the entire boot image (bootloader + kernel)
    ; starting at 0x7C00. The kernel is at 0x7C00 + 2048 (0x8400).
    ; We need to copy it to KERNEL_OFFSET (0x1000).
    
    ; Set up source and destination for memory copy
    ; Source: 0x7C00 + 2048 = 0x8400 (kernel in loaded image)
    ; Dest: 0x1000 (where kernel expects to be)
    ; Size: ~20KB kernel
    
    push es
    push ds
    
    mov ax, 0
    mov es, ax
    mov ds, ax
    
    mov si, 0x8400          ; Source: bootloader end (0x7C00 + 2048)
    mov di, KERNEL_OFFSET   ; Destination: 0x1000
    mov cx, 10240           ; Copy 20KB (10240 words = 20480 bytes)
    
    cld                     ; Clear direction flag (forward copy)
    rep movsw               ; Copy words from DS:SI to ES:DI
    
    pop ds
    pop es
    
    popa
    ret

; ============================================
; GDT (Global Descriptor Table)
; ============================================

gdt_start:
    ; Null descriptor
    dq 0

gdt_code:
    ; Code segment: base=0, limit=0xFFFFF, 4KB granularity, 32-bit, executable
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0000       ; Base (bits 0-15)
    db 0x00         ; Base (bits 16-23)
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit (bits 16-19)
    db 0x00         ; Base (bits 24-31)

gdt_data:
    ; Data segment: same as code but not executable
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b    ; Access byte (data segment)
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1  ; Size
    dd gdt_start                 ; Address

CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ============================================
; Data
; ============================================

BOOT_DRIVE:   db 0
MSG_BOOT:     db 'OS/2 Clone ISO Bootloader v1.0', 13, 10, 0
MSG_LOAD:     db 'Loading kernel...', 13, 10, 0
MSG_PMODE:    db 'Entering protected mode...', 13, 10, 0

; ============================================
; 32-bit Protected Mode Entry
; ============================================

[BITS 32]
protected_mode_start:
    ; Set up segment registers for protected mode
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000        ; Set up stack

    ; Jump to kernel
    jmp CODE_SEG:KERNEL_OFFSET

; ============================================
; Padding - El Torito no-emulation boot image
; For no-emulation mode, we don't strictly need 2048 byte alignment
; but we pad to make the image cleaner
; ============================================

times 2048 - ($ - $$) db 0
