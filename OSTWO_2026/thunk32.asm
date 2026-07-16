; File: thunk32.asm
; 32-bit DOSCALLS thunks for OS/2 LX apps running in the 64-bit kernel's
; compatibility mode. Each thunk is padded to a fixed 32-byte slot so
; the loader can address slot i at THUNK_BASE + i*32 without needing a
; symbol table. Assembled with: nasm -f bin -> flat 32-bit machine code.
;
; OS/2 _System convention: args pushed right-to-left, caller cleans up,
; APIRET in EAX (0 = NO_ERROR), EBX/ESI preserved.
; OSTwo int 0x80 ABI: EAX = number, EBX/ECX/EDX = args.

[BITS 32]

SLOT equ 32

; --- slot 0: DosWrite(handle, buf, len, &written)  [ord 282] ---
    push ebx
    mov ebx, [esp + 8]          ; handle
    mov ecx, [esp + 12]         ; buffer
    mov edx, [esp + 16]         ; length
    mov eax, 2                  ; SYSCALL_WRITE
    int 0x80
    mov ecx, [esp + 20]         ; &written
    jecxz $ + 4
    mov [ecx], eax
    pop ebx
    xor eax, eax
    ret
    times SLOT-($-$$) db 0

; --- slot 1: DosExit(action, result)  [ord 234] ---
    mov ebx, [esp + 8]          ; result
    mov eax, 1                  ; SYSCALL_EXIT
    int 0x80
    jmp $                       ; never returns
    times SLOT*2-($-$$) db 0

; --- slot 2: DosBeep(freq, dur)  [ord 286] ---
    push ebx
    mov ebx, [esp + 8]          ; frequency
    mov ecx, [esp + 12]         ; duration
    mov eax, 8                  ; SYSCALL_BEEP
    int 0x80
    pop ebx
    xor eax, eax
    ret
    times SLOT*3-($-$$) db 0

; --- slot 3: DosSleep(msec)  [ord 229] ---
    push ebx
    mov ebx, [esp + 8]
    mov eax, 7                  ; SYSCALL_SLEEP
    int 0x80
    pop ebx
    xor eax, eax
    ret
    times SLOT*4-($-$$) db 0

; --- slot 4: DosRead(handle, buf, len, &read)  [ord 281] ---
    push ebx
    mov ebx, [esp + 8]
    mov ecx, [esp + 12]
    mov edx, [esp + 16]
    mov eax, 3                  ; SYSCALL_READ
    int 0x80
    mov ecx, [esp + 20]
    jecxz $ + 4
    mov [ecx], eax
    pop ebx
    xor eax, eax
    ret
    times SLOT*5-($-$$) db 0
