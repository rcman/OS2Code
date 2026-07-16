// File: semaphore.h
// Process synchronization - Semaphores and Mutexes

#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include "types.h"

// Semaphore types
#define SEM_TYPE_MUTEX  1    // Mutex semaphore (binary lock)
#define SEM_TYPE_EVENT  2    // Event semaphore (signaling)

// Semaphore states
#define SEM_STATE_LOCKED    0    // Mutex is locked / Event is cleared
#define SEM_STATE_UNLOCKED  1    // Mutex is unlocked / Event is set

// Maximum number of semaphores
#define MAX_SEMAPHORES 64

// Maximum processes waiting on a semaphore
#define MAX_SEM_WAITERS 16

// Semaphore structure
typedef struct {
    uint32_t id;                        // Semaphore handle/ID
    uint32_t type;                      // SEM_TYPE_MUTEX or SEM_TYPE_EVENT
    uint32_t state;                     // SEM_STATE_LOCKED or SEM_STATE_UNLOCKED
    uint32_t owner_pid;                 // PID of owner (for mutexes, 0 if unlocked)
    uint32_t in_use;                    // 1 if allocated, 0 if free
    char name[32];                      // Optional name for debugging

    // Wait queue - processes blocked waiting for this semaphore
    uint32_t waiters[MAX_SEM_WAITERS];  // PIDs of waiting processes
    uint32_t waiter_count;              // Number of waiting processes
} semaphore_t;

// Initialize semaphore subsystem
void semaphore_init(void);

// Create a mutex semaphore
// Returns semaphore handle (ID) on success, 0 on failure
uint32_t sem_create_mutex(const char* name, uint32_t initial_state);

// Create an event semaphore
// Returns semaphore handle (ID) on success, 0 on failure
uint32_t sem_create_event(const char* name, uint32_t initial_state);

// Request (acquire) a semaphore
// Blocks the calling process if semaphore is not available
// Returns 0 on success, -1 on error
int32_t sem_request(uint32_t sem_id, uint32_t timeout_ms);

// Clear (release) a semaphore
// For mutex: unlocks and wakes one waiter
// For event: clears the event state
// Returns 0 on success, -1 on error
int32_t sem_clear(uint32_t sem_id);

// Set a semaphore
// For mutex: this is an error (use clear instead)
// For event: sets the event state, waking all waiters
// Returns 0 on success, -1 on error
int32_t sem_set(uint32_t sem_id);

// Close/destroy a semaphore
// Returns 0 on success, -1 on error
int32_t sem_close(uint32_t sem_id);

// Get semaphore by ID
semaphore_t* sem_get(uint32_t sem_id);

// Wake next waiter on a semaphore (internal)
void sem_wake_next_waiter(semaphore_t* sem);

// Print semaphore table (debug)
void sem_print_table(void);

#endif // SEMAPHORE_H
