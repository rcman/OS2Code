; OS/2 Clone Bootloader
; Loads kernel from disk and switches to 32-bit protected mode

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

    ; Save boot drive
    mov [BOOT_DRIVE], dl

    ; Clear screen
    mov ax, 0x0003
    int 0x10

    ; Print welcome message
    mov si, MSG_BOOT
    call print_string

    ; Load kernel from disk
    mov si, MSG_LOAD
    call print_string

    call load_kernel

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

load_kernel:
    pusha
    mov bx, KERNEL_OFFSET   ; Load to es:bx

    ; Read first 17 sectors (sectors 2-18 on track 0, head 0)
    mov ah, 0x02            ; BIOS read sectors
    mov al, 17              ; Read 17 sectors (to end of track)
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Start from sector 2
    mov dh, 0               ; Head 0
    mov dl, [BOOT_DRIVE]    ; Boot drive
    int 0x13
    jc disk_error

    ; Read second batch (20 more sectors - enough for kernel)
    add bx, 17 * 512        ; Move buffer pointer forward
    mov ah, 0x02
    mov al, 20              ; Read 20 more sectors
    mov ch, 0               ; Cylinder 0
    mov cl, 1               ; Sector 1 on next track
    mov dh, 1               ; Head 1
    mov dl, [BOOT_DRIVE]
    int 0x13
    jc disk_error

    popa
    ret

disk_error:
    mov si, MSG_DISK_ERR
    call print_string
    jmp $

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

BOOT_DRIVE: db 0
MSG_BOOT:     db 'OS/2 Clone Bootloader v1.0', 13, 10, 0
MSG_LOAD:     db 'Loading kernel...', 13, 10, 0
MSG_PMODE:    db 'Entering protected mode...', 13, 10, 0
MSG_DISK_ERR: db 'Disk read error!', 13, 10, 0

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
; Boot sector padding and signature
; ============================================

times 510 - ($ - $$) db 0
dw 0xAA55
