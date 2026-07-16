; File: switch.asm
; Context switching assembly code
; Assembled with: nasm -f elf32 switch.asm -o switch.o

[BITS 32]

section .text

; void switch_to_process(process_t* next)
; Switches to the given process
; For first time run (new process), sets up initial context
global switch_to_process
switch_to_process:
    cli                          ; Disable interrupts during switch
    mov eax, [esp + 4]          ; Get next process pointer

    ; Save process pointer in edx
    mov edx, eax

    ; Load segment registers from process
    mov bx, [edx + 84]          ; ds
    mov ds, bx
    mov bx, [edx + 88]          ; es
    mov es, bx
    mov bx, [edx + 92]          ; fs
    mov fs, bx
    mov bx, [edx + 96]          ; gs
    mov gs, bx

    ; Set up temporary stack to push iret frame
    ; Use kernel stack or current stack
    ; Push values for iret: SS, ESP, EFLAGS, CS, EIP
    push dword [edx + 100]      ; SS
    push dword [edx + 44]       ; ESP
    push dword [edx + 76]       ; EFLAGS
    push dword [edx + 80]       ; CS
    push dword [edx + 40]       ; EIP

    ; Load general purpose registers (use edx as source)
    mov ebx, [edx + 56]
    mov ecx, [edx + 60]
    mov esi, [edx + 68]
    mov edi, [edx + 72]
    mov ebp, [edx + 48]
    mov eax, [edx + 52]

    ; Load edx last
    mov edx, [edx + 64]

    ; Jump to process (enables interrupts via EFLAGS)
    iret

; Save current process context (called from timer interrupt)
; void save_current_context(registers_t* regs, process_t* proc)
global save_current_context
save_current_context:
    push ebp
    mov ebp, esp

    mov eax, [ebp + 8]          ; registers_t* regs
    mov edx, [ebp + 12]         ; process_t* proc

    ; Save general purpose registers from interrupt frame
    mov ecx, [eax + 32]         ; regs->eax
    mov [edx + 52], ecx
    mov ecx, [eax + 16]         ; regs->ebx
    mov [edx + 56], ecx
    mov ecx, [eax + 28]         ; regs->ecx
    mov [edx + 60], ecx
    mov ecx, [eax + 24]         ; regs->edx
    mov [edx + 64], ecx
    mov ecx, [eax + 8]          ; regs->esi
    mov [edx + 68], ecx
    mov ecx, [eax + 4]          ; regs->edi
    mov [edx + 72], ecx

    ; Save EIP, ESP, EBP, EFLAGS from interrupt frame
    mov ecx, [eax + 44]         ; regs->eip
    mov [edx + 40], ecx
    mov ecx, [eax + 12]         ; regs->esp
    mov [edx + 44], ecx
    mov ecx, [eax + 20]         ; regs->ebp
    mov [edx + 48], ecx
    mov ecx, [eax + 48]         ; regs->eflags
    mov [edx + 76], ecx

    ; Save segment registers
    mov ecx, [eax + 0]          ; regs->ds
    mov [edx + 84], ecx
    mov ecx, [eax + 46]         ; regs->cs
    mov [edx + 80], ecx
    mov ecx, [eax + 52]         ; regs->ss (from useresp)
    mov [edx + 100], ecx

    ; ES, FS, GS - use DS value for now
    mov [edx + 88], ecx         ; es
    mov [edx + 92], ecx         ; fs
    mov [edx + 96], ecx         ; gs

    pop ebp
    ret

; ============================================================
; Kernel context save/restore (setjmp/longjmp style)
; Used by run_process_and_wait / exit_to_kernel so that
; returning from a user process to the kernel is well-defined
; even under compiler optimization (replaces the old
; computed-goto hack which miscompiled at -O2).
;
; Context layout (6 dwords): ebx, esi, edi, ebp, esp, eip
; ============================================================

global kctx_save
global kctx_restore

; int kctx_save(uint32_t ctx[6])
; Returns 0 when called directly, 1 when resumed via kctx_restore.
kctx_save:
    mov eax, [esp + 4]          ; ctx pointer
    mov [eax + 0],  ebx
    mov [eax + 4],  esi
    mov [eax + 8],  edi
    mov [eax + 12], ebp
    lea ecx, [esp + 4]          ; esp as it will be after ret
    mov [eax + 16], ecx
    mov ecx, [esp]              ; return address
    mov [eax + 20], ecx
    xor eax, eax                ; return 0 (direct call)
    ret

; void kctx_restore(uint32_t ctx[6])  -- does not return
; Resumes execution after the matching kctx_save, which returns 1.
kctx_restore:
    mov eax, [esp + 4]          ; ctx pointer
    mov ebx, [eax + 0]
    mov esi, [eax + 4]
    mov edi, [eax + 8]
    mov ebp, [eax + 12]
    mov esp, [eax + 16]
    mov ecx, [eax + 20]
    mov eax, 1                  ; kctx_save returns 1 this time
    jmp ecx

section .note.GNU-stack noalloc noexec nowrite progbits
