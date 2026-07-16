// File: semaphore.c
// Process synchronization - Semaphore implementation

#include "semaphore.h"
#include "process.h"

// External functions
extern void printf(const char* format, ...);

// Semaphore table
static semaphore_t semaphore_table[MAX_SEMAPHORES];
static uint32_t next_sem_id = 1;  // Start from 1 (0 = invalid)

// Initialize semaphore subsystem
void semaphore_init(void) {
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        semaphore_table[i].in_use = 0;
        semaphore_table[i].id = 0;
        semaphore_table[i].type = 0;
        semaphore_table[i].state = SEM_STATE_UNLOCKED;
        semaphore_table[i].owner_pid = 0;
        semaphore_table[i].waiter_count = 0;
        for (int j = 0; j < MAX_SEM_WAITERS; j++) {
            semaphore_table[i].waiters[j] = 0;
        }
        for (int j = 0; j < 32; j++) {
            semaphore_table[i].name[j] = 0;
        }
    }
    printf("[Semaphore] Initialized (max %d semaphores)\n", MAX_SEMAPHORES);
}

// Get semaphore by ID
semaphore_t* sem_get(uint32_t sem_id) {
    if (sem_id == 0) {
        return NULL;
    }

    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphore_table[i].in_use && semaphore_table[i].id == sem_id) {
            return &semaphore_table[i];
        }
    }
    return NULL;
}

// Create a mutex semaphore
uint32_t sem_create_mutex(const char* name, uint32_t initial_state) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (!semaphore_table[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("[Semaphore] ERROR: No free semaphore slots\n");
        return 0;
    }

    // Initialize semaphore
    semaphore_table[slot].id = next_sem_id++;
    semaphore_table[slot].type = SEM_TYPE_MUTEX;
    semaphore_table[slot].state = initial_state ? SEM_STATE_UNLOCKED : SEM_STATE_LOCKED;
    semaphore_table[slot].owner_pid = 0;
    semaphore_table[slot].waiter_count = 0;
    semaphore_table[slot].in_use = 1;

    // Copy name
    if (name) {
        int i;
        for (i = 0; i < 31 && name[i]; i++) {
            semaphore_table[slot].name[i] = name[i];
        }
        semaphore_table[slot].name[i] = '\0';
    } else {
        semaphore_table[slot].name[0] = '\0';
    }

    printf("[Semaphore] Created mutex sem=%d name='%s' state=%d\n",
           semaphore_table[slot].id,
           semaphore_table[slot].name,
           semaphore_table[slot].state);

    return semaphore_table[slot].id;
}

// Create an event semaphore
uint32_t sem_create_event(const char* name, uint32_t initial_state) {
    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (!semaphore_table[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("[Semaphore] ERROR: No free semaphore slots\n");
        return 0;
    }

    // Initialize semaphore
    semaphore_table[slot].id = next_sem_id++;
    semaphore_table[slot].type = SEM_TYPE_EVENT;
    semaphore_table[slot].state = initial_state ? SEM_STATE_UNLOCKED : SEM_STATE_LOCKED;
    semaphore_table[slot].owner_pid = 0;
    semaphore_table[slot].waiter_count = 0;
    semaphore_table[slot].in_use = 1;

    // Copy name
    if (name) {
        int i;
        for (i = 0; i < 31 && name[i]; i++) {
            semaphore_table[slot].name[i] = name[i];
        }
        semaphore_table[slot].name[i] = '\0';
    } else {
        semaphore_table[slot].name[0] = '\0';
    }

    printf("[Semaphore] Created event sem=%d name='%s' state=%d\n",
           semaphore_table[slot].id,
           semaphore_table[slot].name,
           semaphore_table[slot].state);

    return semaphore_table[slot].id;
}

// Request (acquire) a semaphore
int32_t sem_request(uint32_t sem_id, uint32_t timeout_ms) {
    (void)timeout_ms;  // TODO: Implement timeout in Phase 2

    semaphore_t* sem = sem_get(sem_id);
    if (!sem) {
        printf("[Semaphore] ERROR: Invalid semaphore ID %d\n", sem_id);
        return -1;
    }

    // Get current process
    extern process_t* process_current(void);
    process_t* proc = process_current();
    if (!proc) {
        printf("[Semaphore] ERROR: No current process\n");
        return -1;
    }

    if (sem->type == SEM_TYPE_MUTEX) {
        // Mutex semaphore
        if (sem->state == SEM_STATE_UNLOCKED) {
            // Acquire the mutex
            sem->state = SEM_STATE_LOCKED;
            sem->owner_pid = proc->pid;
            return 0;
        } else {
            // Mutex is locked
            // TODO: In full implementation, block the process here
            // For now, we'll just fail
            printf("[Semaphore] WARNING: Mutex %d is locked (owner PID %d)\n",
                   sem_id, sem->owner_pid);
            return -1;
        }
    } else if (sem->type == SEM_TYPE_EVENT) {
        // Event semaphore - wait for it to be set
        if (sem->state == SEM_STATE_UNLOCKED) {
            // Event is set, proceed
            return 0;
        } else {
            // Event is cleared
            // TODO: In full implementation, block the process here
            printf("[Semaphore] WARNING: Event %d is cleared (not signaled)\n", sem_id);
            return -1;
        }
    }

    return -1;
}

// Clear (release) a semaphore
int32_t sem_clear(uint32_t sem_id) {
    semaphore_t* sem = sem_get(sem_id);
    if (!sem) {
        printf("[Semaphore] ERROR: Invalid semaphore ID %d\n", sem_id);
        return -1;
    }

    // Get current process
    extern process_t* process_current(void);
    process_t* proc = process_current();
    if (!proc) {
        printf("[Semaphore] ERROR: No current process\n");
        return -1;
    }

    if (sem->type == SEM_TYPE_MUTEX) {
        // Mutex semaphore - release
        if (sem->state == SEM_STATE_LOCKED && sem->owner_pid == proc->pid) {
            // Release the mutex
            sem->state = SEM_STATE_UNLOCKED;
            sem->owner_pid = 0;

            // TODO: Wake next waiter if any
            if (sem->waiter_count > 0) {
                sem_wake_next_waiter(sem);
            }

            return 0;
        } else if (sem->state == SEM_STATE_UNLOCKED) {
            printf("[Semaphore] WARNING: Mutex %d already unlocked\n", sem_id);
            return -1;
        } else {
            printf("[Semaphore] ERROR: Process %d doesn't own mutex %d (owner: %d)\n",
                   proc->pid, sem_id, sem->owner_pid);
            return -1;
        }
    } else if (sem->type == SEM_TYPE_EVENT) {
        // Event semaphore - clear the event
        sem->state = SEM_STATE_LOCKED;
        return 0;
    }

    return -1;
}

// Set a semaphore
int32_t sem_set(uint32_t sem_id) {
    semaphore_t* sem = sem_get(sem_id);
    if (!sem) {
        printf("[Semaphore] ERROR: Invalid semaphore ID %d\n", sem_id);
        return -1;
    }

    if (sem->type == SEM_TYPE_MUTEX) {
        // For mutex, "set" doesn't make sense
        printf("[Semaphore] ERROR: Cannot set a mutex semaphore\n");
        return -1;
    } else if (sem->type == SEM_TYPE_EVENT) {
        // Event semaphore - set (signal) the event
        sem->state = SEM_STATE_UNLOCKED;

        // Wake all waiters
        while (sem->waiter_count > 0) {
            sem_wake_next_waiter(sem);
        }

        return 0;
    }

    return -1;
}

// Close/destroy a semaphore
int32_t sem_close(uint32_t sem_id) {
    semaphore_t* sem = sem_get(sem_id);
    if (!sem) {
        printf("[Semaphore] ERROR: Invalid semaphore ID %d\n", sem_id);
        return -1;
    }

    // TODO: Wake all waiters with error code

    // Mark as free
    sem->in_use = 0;
    sem->id = 0;
    sem->waiter_count = 0;

    printf("[Semaphore] Closed semaphore %d\n", sem_id);
    return 0;
}

// Wake next waiter on a semaphore (internal)
void sem_wake_next_waiter(semaphore_t* sem) {
    if (sem->waiter_count == 0) {
        return;
    }

    // Get first waiter PID
    uint32_t waiter_pid = sem->waiters[0];

    // Remove from wait queue (shift array)
    for (uint32_t i = 0; i < sem->waiter_count - 1; i++) {
        sem->waiters[i] = sem->waiters[i + 1];
    }
    sem->waiter_count--;

    // TODO: In full implementation, unblock the process
    // For now, just log it
    printf("[Semaphore] Would wake process %d from semaphore %d\n",
           waiter_pid, sem->id);
}

// Print semaphore table (debug)
void sem_print_table(void) {
    printf("\n=== Semaphore Table ===\n");
    printf("ID   Type   State      Owner  Waiters  Name\n");
    printf("---  -----  ---------  -----  -------  ----\n");

    int count = 0;
    for (int i = 0; i < MAX_SEMAPHORES; i++) {
        if (semaphore_table[i].in_use) {
            const char* type_str = (semaphore_table[i].type == SEM_TYPE_MUTEX) ? "MUTEX" : "EVENT";
            const char* state_str = (semaphore_table[i].state == SEM_STATE_LOCKED) ? "LOCKED" : "UNLOCKED";

            printf("%-4d %-5s  %-9s  %-5d  %-7d  %s\n",
                   semaphore_table[i].id,
                   type_str,
                   state_str,
                   semaphore_table[i].owner_pid,
                   semaphore_table[i].waiter_count,
                   semaphore_table[i].name);
            count++;
        }
    }

    if (count == 0) {
        printf("(no semaphores)\n");
    }
    printf("\n");
}
