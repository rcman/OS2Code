// File: scheduler.c
// Round-robin process scheduler

#include "scheduler.h"
#include "process.h"
#include "vmm.h"

// External functions
extern void printf(const char* format, ...);
extern void process_set_current(process_t* proc);
extern process_t* process_current(void);

// Scheduling enabled flag
static bool scheduling_enabled = false;

// Context switch function (implemented in switch.asm)
extern void switch_to_process(process_t* next);

// Initialize scheduler
void scheduler_init(void) {
    printf("[Scheduler] Initializing round-robin scheduler...\n");
    scheduling_enabled = false;
    printf("[Scheduler] Initialized (disabled until started)\n");
}

// Start scheduling
void scheduler_start(void) {
    printf("[Scheduler] Starting scheduler...\n");
    scheduling_enabled = true;
}

// Get process table and max
extern process_t* process_get_table(void);
extern int process_get_max(void);

// Find next ready process (round-robin)
static process_t* find_next_process(void) {
    process_t* current = process_current();
    process_t* table = process_get_table();
    int max = process_get_max();

    // Start from current PID + 1
    uint32_t start_pid = current ? current->pid + 1 : 1;

    // Search from start_pid to MAX_PROCESSES
    for (int i = 0; i < max * 2; i++) {
        int idx = (start_pid + i) % max;
        process_t* proc = &table[idx];

        if (proc->state == PROCESS_STATE_READY && proc->pid != 0) {
            return proc;
        }
    }

    // No ready process found, return idle (PID 0)
    return &table[0];
}

// Schedule next process
void scheduler_schedule(void) {
    if (!scheduling_enabled) {
        return;
    }

    process_t* current = process_current();

    // Find next process to run
    process_t* next = find_next_process();

    if (!next) {
        // No process found, stay with current
        return;
    }

    // If same process, nothing to do
    if (next == current) {
        return;
    }

    // Update states
    if (current && current->state == PROCESS_STATE_RUNNING) {
        current->state = PROCESS_STATE_READY;
    }

    next->state = PROCESS_STATE_RUNNING;
    next->time_slice = 10;  // Reset time slice

    // Set as current process
    process_set_current(next);

    // Switch page directory
    if (next->page_directory != vmm_get_current_directory()) {
        vmm_switch_page_directory(next->page_directory);
    }

    // Perform context switch (assembly)
    switch_to_process(next);
}

// Called from timer interrupt
void scheduler_tick(void) {
    if (!scheduling_enabled) {
        return;
    }

    process_t* current = process_current();
    if (!current) {
        return;
    }

    // Decrement time slice
    if (current->time_slice > 0) {
        current->time_slice--;
    }

    // Time slice expired?
    if (current->time_slice == 0) {
        scheduler_schedule();
    }
}

// Add process to ready queue (for now, just set state)
void scheduler_add_process(process_t* proc) {
    if (proc) {
        proc->state = PROCESS_STATE_READY;
    }
}

// Remove process from ready queue
void scheduler_remove_process(process_t* proc) {
    if (proc) {
        proc->state = PROCESS_STATE_BLOCKED;
    }
}
