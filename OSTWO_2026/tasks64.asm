; File: tasks64.asm
; Ring-3 task body for the 64-bit preemptive-scheduler demo. One shared,
; position-independent blob; each task is launched with a distinct
; character in RBX and an iteration count in RBP. Between prints the task
; spins in user mode, where the 100 Hz timer preempts it and the
; scheduler round-robins to the next task - so the printed characters
; interleave, proving preemption.

[BITS 64]

section .rodata
global sched_task_blob_start
global sched_task_blob_end

sched_task_blob_start:
.loop:
    mov rax, 5              ; SYS_PUTC
    mov rdi, rbx            ; this task's character (preset at launch)
    syscall                 ; syscall masks IF, so no preemption mid-print
    mov rcx, 0x8000000      ; user-mode busy delay ~ a couple time slices
.delay:
    dec rcx
    jnz .delay
    dec rbp                 ; iterations remaining
    jnz .loop
    mov rax, 3              ; SYS_EXIT
    xor rdi, rdi
    syscall
.hang:
    jmp .hang
sched_task_blob_end:

section .note.GNU-stack noalloc noexec nowrite progbits
