// File: scheduler.h
// Process scheduler

#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "types.h"
#include "process.h"

// Initialize scheduler
void scheduler_init(void);

// Schedule next process (called from timer interrupt)
void scheduler_tick(void);

// Add process to ready queue
void scheduler_add_process(process_t* proc);

// Remove process from ready queue
void scheduler_remove_process(process_t* proc);

// Force a schedule (yield current process)
void scheduler_schedule(void);

#endif // SCHEDULER_H
