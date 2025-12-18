; File: isr.asm
; Interrupt Service Routines
; Assembled with: nasm -f elf32 isr.asm -o isr.o

[BITS 32]

section .text

; External C handlers
extern isr_handler
extern irq_handler

; === Macro for ISRs without error code ===
%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0                    ; Dummy error code
    push dword %1                   ; Interrupt number
    jmp isr_common_stub
%endmacro

; === Macro for ISRs with error code ===
%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1                   ; Interrupt number (error code already pushed by CPU)
    jmp isr_common_stub
%endmacro

; === Macro for IRQs ===
%macro IRQ 2
global irq%1
irq%1:
    push dword 0                    ; Dummy error code
    push dword %2                   ; Interrupt number
    jmp irq_common_stub
%endmacro

; === CPU Exception ISRs (0-31) ===
ISR_NOERRCODE 0                     ; Division By Zero
ISR_NOERRCODE 1                     ; Debug
ISR_NOERRCODE 2                     ; Non Maskable Interrupt
ISR_NOERRCODE 3                     ; Breakpoint
ISR_NOERRCODE 4                     ; Overflow
ISR_NOERRCODE 5                     ; Bound Range Exceeded
ISR_NOERRCODE 6                     ; Invalid Opcode
ISR_NOERRCODE 7                     ; Device Not Available
ISR_ERRCODE   8                     ; Double Fault
ISR_NOERRCODE 9                     ; Coprocessor Segment Overrun
ISR_ERRCODE   10                    ; Invalid TSS
ISR_ERRCODE   11                    ; Segment Not Present
ISR_ERRCODE   12                    ; Stack Fault
ISR_ERRCODE   13                    ; General Protection Fault
ISR_ERRCODE   14                    ; Page Fault
ISR_NOERRCODE 15                    ; Reserved
ISR_NOERRCODE 16                    ; x87 Floating Point Exception
ISR_ERRCODE   17                    ; Alignment Check
ISR_NOERRCODE 18                    ; Machine Check
ISR_NOERRCODE 19                    ; SIMD Floating Point Exception
ISR_NOERRCODE 20                    ; Virtualization Exception
ISR_NOERRCODE 21                    ; Reserved
ISR_NOERRCODE 22                    ; Reserved
ISR_NOERRCODE 23                    ; Reserved
ISR_NOERRCODE 24                    ; Reserved
ISR_NOERRCODE 25                    ; Reserved
ISR_NOERRCODE 26                    ; Reserved
ISR_NOERRCODE 27                    ; Reserved
ISR_NOERRCODE 28                    ; Reserved
ISR_NOERRCODE 29                    ; Reserved
ISR_ERRCODE   30                    ; Security Exception
ISR_NOERRCODE 31                    ; Reserved

; === Hardware IRQs (32-47) ===
IRQ 0, 32                           ; Timer
IRQ 1, 33                           ; Keyboard
IRQ 2, 34                           ; Cascade
IRQ 3, 35                           ; COM2
IRQ 4, 36                           ; COM1
IRQ 5, 37                           ; LPT2
IRQ 6, 38                           ; Floppy
IRQ 7, 39                           ; LPT1
IRQ 8, 40                           ; CMOS RTC
IRQ 9, 41                           ; Free
IRQ 10, 42                          ; Free
IRQ 11, 43                          ; Free
IRQ 12, 44                          ; PS/2 Mouse
IRQ 13, 45                          ; FPU
IRQ 14, 46                          ; Primary ATA
IRQ 15, 47                          ; Secondary ATA

; === Syscall (INT 0x80) ===
global isr128
isr128:
    push dword 0
    push dword 128
    jmp isr_common_stub

; === Common ISR stub ===
isr_common_stub:
    ; Save all registers
    pusha                           ; Pushes EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    
    ; Save segment registers
    mov ax, ds
    push eax
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push pointer to register structure
    push esp
    
    ; Call C handler
    call isr_handler
    
    ; Remove pushed parameter
    add esp, 4
    
    ; Restore segment registers
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Restore general registers
    popa
    
    ; Clean up pushed error code and ISR number
    add esp, 8
    
    ; Return from interrupt
    iret

; === Common IRQ stub ===
irq_common_stub:
    ; Save all registers
    pusha
    
    ; Save segment registers
    mov ax, ds
    push eax
    
    ; Load kernel data segment
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Push pointer to register structure
    push esp
    
    ; Call C handler
    call irq_handler
    
    ; Remove pushed parameter
    add esp, 4
    
    ; Restore segment registers
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    ; Restore general registers
    popa
    
    ; Clean up pushed error code and IRQ number
    add esp, 8
    
    ; Return from interrupt
    iret
