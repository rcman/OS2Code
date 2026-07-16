// File: thread.h
// Thread management - Multi-threading support

#ifndef THREAD_H
#define THREAD_H

#include "types.h"

// Thread states
#define THREAD_STATE_UNUSED      0
#define THREAD_STATE_READY       1
#define THREAD_STATE_RUNNING     2
#define THREAD_STATE_BLOCKED     3
#define THREAD_STATE_TERMINATED  4

// Maximum number of threads
#define MAX_THREADS 64

// Thread stack size (16KB per thread)
#define THREAD_STACK_SIZE (16 * 1024)

// Thread Control Block (TCB)
typedef struct {
    uint32_t tid;                   // Thread ID
    uint32_t pid;                   // Parent process ID
    uint32_t state;                 // Thread state
    char name[32];                  // Thread name

    // CPU context (saved during context switch)
    uint32_t eip;                   // Instruction pointer
    uint32_t esp;                   // Stack pointer
    uint32_t ebp;                   // Base pointer
    uint32_t eax, ebx, ecx, edx;    // General purpose registers
    uint32_t esi, edi;              // Index registers
    uint32_t eflags;                // CPU flags

    // Segment registers (share with parent process)
    uint32_t cs, ds, es, fs, gs, ss;

    // Thread-specific resources
    uint32_t stack_base;            // Base of thread stack (virtual address)
    uint32_t stack_size;            // Stack size in bytes
    uint32_t kernel_stack;          // Kernel stack for this thread

    // Scheduling
    uint32_t priority;              // Thread priority (inherited from process)
    uint32_t time_slice;            // Remaining time slice
    uint32_t total_time;            // Total CPU time used

    // Thread entry point and argument
    void (*entry_point)(void* arg);
    void* arg;

    // Exit status
    uint32_t exit_code;
} thread_t;

// Initialize thread subsystem
void thread_init(void);

// Create a new thread in the current process
// Returns TID on success, 0 on failure
uint32_t thread_create(const char* name, void (*entry_point)(void* arg), void* arg, uint32_t priority);

// Exit current thread
void thread_exit(uint32_t exit_code);

// Get current running thread
thread_t* thread_current(void);

// Get thread by TID
thread_t* thread_get(uint32_t tid);

// Get thread ID
uint32_t thread_get_tid(thread_t* thread);

// Yield CPU to next thread
void thread_yield(void);

// Print thread table
void thread_print_table(void);

// Thread wrapper (internal - used to start threads)
void thread_wrapper(void);

#endif // THREAD_H
