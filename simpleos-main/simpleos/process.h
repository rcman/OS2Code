// File: process.h
// Process management - PCB and process operations

#ifndef PROCESS_H
#define PROCESS_H

#include "types.h"

// Process states
#define PROCESS_STATE_UNUSED      0
#define PROCESS_STATE_READY       1
#define PROCESS_STATE_RUNNING     2
#define PROCESS_STATE_BLOCKED     3
#define PROCESS_STATE_TERMINATED  4

// Maximum number of processes
#define MAX_PROCESSES 32

// Process priority levels (OS/2 has 4 classes, we'll start simple)
#define PRIORITY_IDLE      0
#define PRIORITY_REGULAR   1
#define PRIORITY_HIGH      2
#define PRIORITY_REALTIME  3

// Process Control Block (PCB)
typedef struct {
    uint32_t pid;                   // Process ID
    uint32_t state;                 // Process state
    char name[32];                  // Process name

    // CPU context (saved during context switch)
    uint32_t eip;                   // Instruction pointer
    uint32_t esp;                   // Stack pointer
    uint32_t ebp;                   // Base pointer
    uint32_t eax, ebx, ecx, edx;    // General purpose registers
    uint32_t esi, edi;              // Index registers
    uint32_t eflags;                // CPU flags

    // Segment registers
    uint32_t cs, ds, es, fs, gs, ss;

    // Memory management
    uint32_t page_directory;        // Physical address of page directory
    uint32_t kernel_stack;          // Kernel stack for this process
    uint32_t user_stack;            // User stack pointer

    // Scheduling
    uint32_t priority;              // Process priority
    uint32_t time_slice;            // Remaining time slice (in ticks)
    uint32_t total_time;            // Total CPU time used

    // Parent/child relationships
    uint32_t parent_pid;            // Parent process ID
    uint32_t exit_code;             // Exit code when terminated
    uint32_t child_count;           // Number of children
} process_t;

// Initialize process management
void process_init(void);

// Create a new process
// Returns PID on success, 0 on failure
uint32_t process_create(const char* name, void (*entry_point)(void), uint32_t priority);

// Terminate a process
void process_exit(uint32_t pid);

// Get current running process
process_t* process_current(void);

// Get process by PID
process_t* process_get(uint32_t pid);

// Yield CPU to next process
void process_yield(void);

// Print process table
void process_print_table(void);

// Get next available PID
uint32_t process_next_pid(void);

// Get parent PID of a process
uint32_t process_get_parent(uint32_t pid);

// Get number of children
uint32_t process_get_child_count(uint32_t pid);

// List all child PIDs (returns count, fills array)
uint32_t process_list_children(uint32_t pid, uint32_t* child_pids, uint32_t max);

#endif // PROCESS_H
