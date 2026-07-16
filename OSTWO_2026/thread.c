// File: thread.c
// Thread management implementation

#include "thread.h"
#include "process.h"
#include "vmm.h"

// External functions
extern void printf(const char* format, ...);
extern void* pmm_alloc_page(void);
extern void pmm_free_page(uint32_t addr);

// Thread table
static thread_t thread_table[MAX_THREADS];
static uint32_t next_tid = 1;  // Start from 1 (0 = invalid)
static thread_t* current_thread = NULL;

// Initialize thread subsystem
void thread_init(void) {
    for (int i = 0; i < MAX_THREADS; i++) {
        thread_table[i].state = THREAD_STATE_UNUSED;
        thread_table[i].tid = 0;
        thread_table[i].pid = 0;
    }
    printf("[Thread] Initialized (max %d threads)\n", MAX_THREADS);
}

// Get current running thread
thread_t* thread_current(void) {
    return current_thread;
}

// Get thread by TID
thread_t* thread_get(uint32_t tid) {
    if (tid == 0) {
        return NULL;
    }

    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state != THREAD_STATE_UNUSED &&
            thread_table[i].tid == tid) {
            return &thread_table[i];
        }
    }
    return NULL;
}

// Get thread ID
uint32_t thread_get_tid(thread_t* thread) {
    if (!thread) {
        return 0;
    }
    return thread->tid;
}

// Thread wrapper - called when a thread starts
// This function calls the thread entry point and handles thread exit
void thread_wrapper(void) {
    thread_t* thread = thread_current();
    if (!thread) {
        printf("[Thread] ERROR: No current thread in wrapper!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    printf("[Thread] TID %d starting: %s\n", thread->tid, thread->name);

    // Call the thread entry point
    if (thread->entry_point) {
        thread->entry_point(thread->arg);
    }

    // Thread returned normally - exit with code 0
    printf("[Thread] TID %d exiting normally\n", thread->tid);
    thread_exit(0);
}

// Create a new thread in the current process
uint32_t thread_create(const char* name, void (*entry_point)(void* arg), void* arg, uint32_t priority) {
    if (!entry_point) {
        printf("[Thread] ERROR: NULL entry point\n");
        return 0;
    }

    // Get current process
    extern process_t* process_current(void);
    process_t* proc = process_current();
    if (!proc) {
        printf("[Thread] ERROR: No current process\n");
        return 0;
    }

    // Find free thread slot
    int slot = -1;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state == THREAD_STATE_UNUSED) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("[Thread] ERROR: No free thread slots\n");
        return 0;
    }

    thread_t* thread = &thread_table[slot];

    // Initialize thread
    thread->tid = next_tid++;
    thread->pid = proc->pid;
    thread->state = THREAD_STATE_READY;
    thread->priority = priority;
    thread->time_slice = 10;  // 10 timer ticks
    thread->total_time = 0;
    thread->exit_code = 0;
    thread->entry_point = entry_point;
    thread->arg = arg;

    // Copy name
    int i;
    for (i = 0; i < 31 && name && name[i]; i++) {
        thread->name[i] = name[i];
    }
    thread->name[i] = '\0';

    // Allocate stack for thread (use process memory allocation)
    extern void* process_alloc_mem(process_t* proc, uint32_t size_bytes);
    void* stack = process_alloc_mem(proc, THREAD_STACK_SIZE);
    if (!stack) {
        printf("[Thread] ERROR: Failed to allocate stack\n");
        thread->state = THREAD_STATE_UNUSED;
        return 0;
    }

    thread->stack_base = (uint32_t)stack;
    thread->stack_size = THREAD_STACK_SIZE;

    // Set up initial stack pointer (stack grows downward)
    thread->esp = thread->stack_base + THREAD_STACK_SIZE - 4;
    thread->ebp = thread->esp;

    // Set up initial registers
    thread->eip = (uint32_t)thread_wrapper;
    thread->eflags = 0x202;  // IF (interrupts enabled) + reserved bit 1

    // Initialize segment registers (user mode)
    thread->cs = 0x1B;  // User code segment (Ring 3)
    thread->ds = 0x23;  // User data segment (Ring 3)
    thread->es = 0x23;
    thread->fs = 0x23;
    thread->gs = 0x23;
    thread->ss = 0x23;

    // Initialize general purpose registers
    thread->eax = 0;
    thread->ebx = 0;
    thread->ecx = 0;
    thread->edx = 0;
    thread->esi = 0;
    thread->edi = 0;

    // Allocate kernel stack (for syscalls)
    uint32_t kernel_stack_phys = (uint32_t)pmm_alloc_page();
    if (kernel_stack_phys == 0) {
        printf("[Thread] ERROR: Failed to allocate kernel stack\n");
        extern int process_free_mem(process_t* proc, void* addr);
        process_free_mem(proc, stack);
        thread->state = THREAD_STATE_UNUSED;
        return 0;
    }
    thread->kernel_stack = kernel_stack_phys + 4096;  // Top of stack

    printf("[Thread] Created TID %d '%s' in process %d (stack=0x%x)\n",
           thread->tid, thread->name, thread->pid, thread->stack_base);

    return thread->tid;
}

// Exit current thread
void thread_exit(uint32_t exit_code) {
    thread_t* thread = thread_current();
    if (!thread) {
        printf("[Thread] ERROR: No current thread to exit\n");
        return;
    }

    printf("[Thread] TID %d exiting with code %d\n", thread->tid, exit_code);

    thread->state = THREAD_STATE_TERMINATED;
    thread->exit_code = exit_code;

    // TODO: Free thread resources (stack, kernel stack)
    // For now, we'll leave them allocated

    // Yield to scheduler to switch to another thread
    extern void scheduler_schedule(void);
    scheduler_schedule();

    // Should never reach here
    printf("[Thread] ERROR: Returned from scheduler after exit!\n");
    while (1) { __asm__ volatile("hlt"); }
}

// Yield CPU to next thread
void thread_yield(void) {
    extern void scheduler_schedule(void);
    scheduler_schedule();
}

// Print thread table
void thread_print_table(void) {
    printf("\n=== Thread Table ===\n");
    printf("TID   PID   State        Priority  Stack       Name\n");
    printf("----  ----  -----------  --------  ----------  ----\n");

    int count = 0;
    for (int i = 0; i < MAX_THREADS; i++) {
        if (thread_table[i].state != THREAD_STATE_UNUSED) {
            const char* state_str;
            switch (thread_table[i].state) {
                case THREAD_STATE_READY:      state_str = "READY"; break;
                case THREAD_STATE_RUNNING:    state_str = "RUNNING"; break;
                case THREAD_STATE_BLOCKED:    state_str = "BLOCKED"; break;
                case THREAD_STATE_TERMINATED: state_str = "TERMINATED"; break;
                default:                      state_str = "UNKNOWN"; break;
            }

            printf("%-4d  %-4d  %-11s  %-8d  0x%08x  %s\n",
                   thread_table[i].tid,
                   thread_table[i].pid,
                   state_str,
                   thread_table[i].priority,
                   thread_table[i].stack_base,
                   thread_table[i].name);
            count++;
        }
    }

    if (count == 0) {
        printf("(no threads)\n");
    }
    printf("\n");
}
