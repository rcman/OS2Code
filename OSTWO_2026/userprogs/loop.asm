; User program that prints 'X' in a loop and then exits
; Demonstrates a process that runs for a while

[bits 32]
[org 0x40000000]

start:
    ; Print startup message
    mov eax, 2              ; SYSCALL_WRITE
    mov ebx, 1              ; stdout
    mov ecx, start_msg
    mov edx, start_msg_len
    int 0x80

    ; Loop counter
    mov edi, 10             ; Loop 10 times

.loop:
    ; Print 'X'
    mov eax, 2              ; SYSCALL_WRITE
    mov ebx, 1              ; stdout
    mov ecx, char_x
    mov edx, 1
    int 0x80

    ; Busy wait
    mov ecx, 1000000
.busy:
    dec ecx
    jnz .busy

    ; Decrement counter
    dec edi
    jnz .loop

    ; Print exit message
    mov eax, 2              ; SYSCALL_WRITE
    mov ebx, 1              ; stdout
    mov ecx, exit_msg
    mov edx, exit_msg_len
    int 0x80

    ; Exit with code 42
    mov eax, 1              ; SYSCALL_EXIT
    mov ebx, 42             ; exit code
    int 0x80

    hlt

start_msg:
    db "[Loop] Starting...", 10
start_msg_len equ $ - start_msg

char_x:
    db 'X'

exit_msg:
    db 10, "[Loop] Exiting with code 42", 10
exit_msg_len equ $ - exit_msg
