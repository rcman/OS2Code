; File: isr64.asm
; 64-bit interrupt stubs, syscall entry and the embedded ring-3 test
; program for the OS/Two 64-bit kernel (Phase B.1).

[BITS 64]

section .text

extern exception64_handler
extern timer64_handler
extern syscall64_dispatch

; ---------------------------------------------------------------- exceptions
; CPU pushes an error code for vectors 8, 10-14, 17, 21. The stubs
; normalize the stack to: [rsp]=vector, [rsp+8]=error, [rsp+16]=RIP...

%macro ISR_NOERR 1
isr64_stub_%1:
    push qword 0
    push qword %1
    jmp isr64_common
%endmacro

%macro ISR_ERR 1
isr64_stub_%1:
    push qword %1
    jmp isr64_common
%endmacro

%assign v 0
%rep 32
%if v == 8 || (v >= 10 && v <= 14) || v == 17 || v == 21
    ISR_ERR v
%else
    ISR_NOERR v
%endif
%assign v v+1
%endrep

isr64_common:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, [rsp + 15*8]       ; vector
    mov rsi, [rsp + 16*8]       ; error code
    mov rdx, [rsp + 17*8]       ; faulting RIP
    call exception64_handler    ; does not return for fatal faults
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    add rsp, 16                 ; vector + error code
    iretq

; Vector table for C to install into the IDT
section .rodata
global isr64_vector_table
align 8
isr64_vector_table:
%assign v 0
%rep 32
    dq isr64_stub_%[v]
%assign v v+1
%endrep

section .text

; ---------------------------------------------------------------- timer IRQ
global irq64_timer
irq64_timer:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    call timer64_handler
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

; ---------------------------------------------------------------- int 0x80
; Compatibility-mode syscall gate: 32-bit user code (CS.L=0) issues
; INT 0x80 with the classic OSTwo ABI (EAX=nr, EBX/ECX/EDX args).
; Interrupt gates always land in 64-bit mode; IRETQ returns to
; compatibility mode automatically via the pushed 32-bit CS.

extern syscall32_dispatch

global int80_entry
int80_entry:
    push rax
    push rcx
    push rdx
    push rbx
    push rbp
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, [rsp + 14*8]       ; saved EAX = syscall number
    mov rsi, [rsp + 11*8]       ; saved EBX
    mov rdx, [rsp + 13*8]       ; saved ECX
    mov rcx, [rsp + 12*8]       ; saved EDX
    call syscall32_dispatch
    mov [rsp + 14*8], rax       ; return value in EAX
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    iretq

; ---------------------------------------------------------------- scheduler
; Context-switching timer IRQ. Saves all 15 GPRs, hands the stack
; pointer to schedule64() and reloads whatever it returns, so a single
; task switch is just swapping RSP. Frame layout (low->high) matches
; struct frame in sched64.c: r15..rax, then the CPU IRETQ frame.

extern schedule64

global irq64_sched
irq64_sched:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
    mov rdi, rsp
    call schedule64             ; returns stack pointer to resume
    mov rsp, rax
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; ---------------------------------------------------------------- keyboard IRQ
extern kbd64_handler

global irq64_kbd
irq64_kbd:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    call kbd64_handler
    mov al, 0x20
    out 0x20, al               ; EOI
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

; ---------------------------------------------------------------- mouse IRQ
extern mouse64_handler

global irq64_mouse
irq64_mouse:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    call mouse64_handler
    mov al, 0x20
    out 0xA0, al               ; EOI to slave PIC
    out 0x20, al               ; EOI to master PIC
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

; void sched_start_asm(uint64_t rsp) - jump into the first task by
; loading its prebuilt frame and returning to ring 3. Never returns.
global sched_start_asm
sched_start_asm:
    mov rsp, rdi
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    iretq

; ---------------------------------------------------------------- syscall
; SYSCALL arrives with RCX=user RIP, R11=user RFLAGS, on the USER stack.
; Switch to a kernel stack, dispatch, switch back, SYSRET.
; User ABI: RAX = number, RDI/RSI/RDX = arguments, result in RAX.

section .bss
align 16
syscall_stack:  resb 16384
syscall_stack_top:
user_rsp_save:  resq 1

section .text
global syscall64_entry
syscall64_entry:
    mov [user_rsp_save], rsp
    mov rsp, syscall_stack_top
    ; Preserve every register the C dispatcher may clobber: like
    ; Linux, the syscall must appear to trash only RAX, RCX and R11.
    push rcx                    ; user return RIP
    push r11                    ; user RFLAGS
    push rdi
    push rsi
    push rdx
    push r8
    push r9
    push r10
    mov rcx, rdx                ; a3 -> C arg 4
    mov rdx, rsi                ; a2 -> C arg 3
    mov rsi, rdi                ; a1 -> C arg 2
    mov rdi, rax                ; number -> C arg 1
    call syscall64_dispatch     ; result in RAX
    pop r10
    pop r9
    pop r8
    pop rdx
    pop rsi
    pop rdi
    pop r11
    pop rcx
    mov rsp, [user_rsp_save]
    o64 sysret

; ---------------------------------------------------------------- user program
; Position-independent 64-bit ring-3 test program. The kernel copies
; this blob into a user-accessible page and IRETQs to it.

section .rodata
global user64_blob_start
global user64_blob_end

user64_blob_start:
    ; print greeting
    mov rax, 1                  ; SYS_PRINT
    lea rdi, [rel .msg1]
    syscall

    ; print a 64-bit value
    mov rax, 2                  ; SYS_PRINTHEX
    mov rdi, 0xFEEDFACE05271964
    syscall

    ; do some genuinely 64-bit work: sum 1..10000000 (needs > 32 bits)
    xor r8, r8
    mov rcx, 10000000
.loop:
    add r8, rcx
    loop .loop
    mov rax, 2
    mov rdi, r8                 ; 0x2D79883D2000 = 50000005000000
    syscall

    mov rax, 1
    lea rdi, [rel .msg2]
    syscall

    mov rax, 3                  ; SYS_EXIT
    syscall

.msg1: db "Hello from 64-bit ring-3 userspace on OS/Two!", 10, 0
.msg2: db "Computed sum(1..10^7) in a 64-bit register.", 10, 0
user64_blob_end:

section .note.GNU-stack noalloc noexec nowrite progbits
