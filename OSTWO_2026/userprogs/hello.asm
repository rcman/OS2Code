; Simple user-mode program that prints "Hello from user program!" and exits
; This is a flat binary that will be loaded at 0x40000000
; Entry point is at offset 0

[bits 32]
[org 0x40000000]

; Entry point
start:
    ; Print message using DosWrite syscall
    ; DosWrite(handle=1, buffer=msg, length=msg_len)
    mov eax, 2              ; SYSCALL_WRITE
    mov ebx, 1              ; stdout
    mov ecx, msg            ; buffer address
    mov edx, msg_len        ; length
    int 0x80

    ; Exit with code 0
    mov eax, 1              ; SYSCALL_EXIT
    mov ebx, 0              ; exit code
    int 0x80

    ; Should never reach here
    hlt

msg:
    db "Hello from user program!", 10
msg_len equ $ - msg
