// File: types.h
// Common type definitions for the kernel

#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// Register structure for interrupt handlers
typedef struct registers {
    uint32_t ds;                    // Data segment selector
    uint32_t edi, esi, ebp, esp;    // Pushed by pusha
    uint32_t ebx, edx, ecx, eax;    // Pushed by pusha
    uint32_t int_no, err_code;      // Interrupt number and error code
    uint32_t eip, cs, eflags;       // Pushed by CPU
    uint32_t useresp, ss;           // Pushed by CPU (only on privilege change)
} registers_t;

// Interrupt handler function pointer type
typedef void (*isr_t)(registers_t*);

#endif // TYPES_H
