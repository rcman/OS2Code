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
