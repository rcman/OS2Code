// File: ipc.c
// Inter-Process Communication subsystem implementation

#include "ipc.h"
#include "process.h"
#include "vmm.h"

// External functions
extern void printf(const char* format, ...);
extern process_t* process_current(void);

// Simple string length
static uint32_t str_length(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    return len;
}

// Simple memory copy
static void mem_copy(void* dest, const void* src, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    const uint8_t* s = (const uint8_t*)src;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

// Simple memory set
static void mem_set(void* dest, uint8_t val, uint32_t n) {
    uint8_t* d = (uint8_t*)dest;
    for (uint32_t i = 0; i < n; i++) {
        d[i] = val;
    }
}

// Global IPC tables
static pipe_t pipes[MAX_PIPES];
static shm_segment_t shm_segments[MAX_SHARED_MEMORY];
static msg_queue_t msg_queues[MAX_MESSAGE_QUEUES];

// Per-process signal information (indexed by PID)
static signal_info_t signal_table[MAX_PROCESSES];

// Helper: String comparison
static int str_equal(const char* s1, const char* s2) {
    while (*s1 && *s2) {
        if (*s1 != *s2) return 0;
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

// Initialize IPC subsystem
void ipc_init(void) {
    // Initialize pipes
    for (uint32_t i = 0; i < MAX_PIPES; i++) {
        pipes[i].in_use = 0;
        pipes[i].id = i;
        pipes[i].name[0] = '\0';
        pipes[i].read_pos = 0;
        pipes[i].write_pos = 0;
        pipes[i].count = 0;
        pipes[i].flags = 0;
        pipes[i].reader_pid = 0;
        pipes[i].writer_pid = 0;
    }

    // Initialize shared memory
    for (uint32_t i = 0; i < MAX_SHARED_MEMORY; i++) {
        shm_segments[i].in_use = 0;
        shm_segments[i].id = i;
        shm_segments[i].key = 0;
        shm_segments[i].base_addr = NULL;
        shm_segments[i].size = 0;
        shm_segments[i].flags = 0;
        shm_segments[i].ref_count = 0;
        shm_segments[i].creator_pid = 0;
    }

    // Initialize message queues
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        msg_queues[i].in_use = 0;
        msg_queues[i].id = i;
        msg_queues[i].key = 0;
        msg_queues[i].count = 0;
        msg_queues[i].head = 0;
        msg_queues[i].tail = 0;
        msg_queues[i].flags = 0;
    }

    // Initialize signal tables
    for (uint32_t i = 0; i < MAX_PROCESSES; i++) {
        signal_table[i].pending = 0;
        signal_table[i].blocked = 0;
        for (uint32_t j = 0; j < MAX_SIGNALS; j++) {
            signal_table[i].handlers[j] = (signal_handler_t)SIG_DFL;
        }
    }

    printf("[IPC] Subsystem initialized\n");
    printf("[IPC] Pipes: %d, Shared Memory: %d, Message Queues: %d\n",
           MAX_PIPES, MAX_SHARED_MEMORY, MAX_MESSAGE_QUEUES);
}

// ============================================================================
// PIPE OPERATIONS
// ============================================================================

// Create a pipe (anonymous if name is NULL/empty, named otherwise)
uint32_t pipe_create(const char* name, uint32_t flags) {
    // Find free pipe slot
    uint32_t pipe_id = 0;
    for (pipe_id = 0; pipe_id < MAX_PIPES; pipe_id++) {
        if (!pipes[pipe_id].in_use) {
            break;
        }
    }

    if (pipe_id >= MAX_PIPES) {
        return (uint32_t)-1;  // No free pipes
    }

    // Initialize pipe
    pipe_t* pipe = &pipes[pipe_id];
    pipe->in_use = 1;
    pipe->read_pos = 0;
    pipe->write_pos = 0;
    pipe->count = 0;
    pipe->flags = flags;
    pipe->reader_pid = 0;
    pipe->writer_pid = 0;

    // Set name if provided
    if (name && name[0] != '\0') {
        uint32_t len = str_length(name);
        if (len >= 64) len = 63;
        for (uint32_t i = 0; i < len; i++) {
            pipe->name[i] = name[i];
        }
        pipe->name[len] = '\0';
    } else {
        pipe->name[0] = '\0';
    }

    return pipe_id;
}

// Open a named pipe
uint32_t pipe_open(const char* name, uint32_t flags) {
    if (!name || name[0] == '\0') {
        return (uint32_t)-1;  // Invalid name
    }

    // Search for named pipe
    for (uint32_t i = 0; i < MAX_PIPES; i++) {
        if (pipes[i].in_use && str_equal(pipes[i].name, name)) {
            return i;
        }
    }

    return (uint32_t)-1;  // Pipe not found
}

// Read from pipe (blocking or non-blocking)
int pipe_read(uint32_t pipe_id, void* buffer, uint32_t size) {
    if (pipe_id >= MAX_PIPES || !pipes[pipe_id].in_use) {
        return -1;  // Invalid pipe
    }

    pipe_t* pipe = &pipes[pipe_id];
    uint8_t* buf = (uint8_t*)buffer;
    uint32_t bytes_read = 0;

    // Get current process
    process_t* proc = process_current();
    if (proc && pipe->reader_pid == 0) {
        pipe->reader_pid = proc->pid;
    }

    while (bytes_read < size) {
        // Check if data available
        if (pipe->count == 0) {
            if (pipe->flags & PIPE_NONBLOCK) {
                break;  // Non-blocking: return what we have
            }
            // Blocking mode: would need scheduler support to wait
            // For now, just return what we have
            break;
        }

        // Read one byte from circular buffer
        buf[bytes_read++] = pipe->buffer[pipe->read_pos];
        pipe->read_pos = (pipe->read_pos + 1) % PIPE_SIZE;
        pipe->count--;
    }

    return (int)bytes_read;
}

// Write to pipe
int pipe_write(uint32_t pipe_id, const void* buffer, uint32_t size) {
    if (pipe_id >= MAX_PIPES || !pipes[pipe_id].in_use) {
        return -1;  // Invalid pipe
    }

    pipe_t* pipe = &pipes[pipe_id];
    const uint8_t* buf = (const uint8_t*)buffer;
    uint32_t bytes_written = 0;

    // Get current process
    process_t* proc = process_current();
    if (proc && pipe->writer_pid == 0) {
        pipe->writer_pid = proc->pid;
    }

    while (bytes_written < size) {
        // Check if space available
        if (pipe->count >= PIPE_SIZE) {
            if (pipe->flags & PIPE_NONBLOCK) {
                break;  // Non-blocking: return what we wrote
            }
            // Blocking mode: would need scheduler support to wait
            break;
        }

        // Write one byte to circular buffer
        pipe->buffer[pipe->write_pos] = buf[bytes_written++];
        pipe->write_pos = (pipe->write_pos + 1) % PIPE_SIZE;
        pipe->count++;
    }

    return (int)bytes_written;
}

// Close pipe
int pipe_close(uint32_t pipe_id) {
    if (pipe_id >= MAX_PIPES || !pipes[pipe_id].in_use) {
        return -1;  // Invalid pipe
    }

    // Clear pipe
    pipes[pipe_id].in_use = 0;
    pipes[pipe_id].name[0] = '\0';
    pipes[pipe_id].read_pos = 0;
    pipes[pipe_id].write_pos = 0;
    pipes[pipe_id].count = 0;
    pipes[pipe_id].reader_pid = 0;
    pipes[pipe_id].writer_pid = 0;

    return 0;
}

// ============================================================================
// SHARED MEMORY OPERATIONS
// ============================================================================

// Create shared memory segment
uint32_t shm_create(uint32_t key, uint32_t size, uint32_t flags) {
    // Check if segment with this key already exists
    if (!(flags & SHM_EXCL)) {
        for (uint32_t i = 0; i < MAX_SHARED_MEMORY; i++) {
            if (shm_segments[i].in_use && shm_segments[i].key == key) {
                if (flags & SHM_CREATE) {
                    return i;  // Return existing segment
                }
            }
        }
    }

    // Find free segment slot
    uint32_t shm_id = 0;
    for (shm_id = 0; shm_id < MAX_SHARED_MEMORY; shm_id++) {
        if (!shm_segments[shm_id].in_use) {
            break;
        }
    }

    if (shm_id >= MAX_SHARED_MEMORY) {
        return (uint32_t)-1;  // No free segments
    }

    // Allocate physical pages for shared memory
    uint32_t pages_needed = (size + 4095) / 4096;  // Round up to pages

    // Use PMM to allocate physical page
    extern uint32_t pmm_alloc_page(void);
    uint32_t base_addr = pmm_alloc_page();  // Allocate first page

    if (base_addr == 0) {
        return (uint32_t)-1;  // Allocation failed
    }

    // Allocate additional pages if needed (contiguous allocation simplified)
    for (uint32_t i = 1; i < pages_needed; i++) {
        uint32_t page = pmm_alloc_page();
        if (page == 0) {
            // Cleanup: free already allocated pages
            // (Simplified - should free all allocated pages)
            extern void pmm_free_page(uint32_t addr);
            pmm_free_page(base_addr);
            return (uint32_t)-1;
        }
    }

    // Initialize segment
    shm_segment_t* shm = &shm_segments[shm_id];
    shm->in_use = 1;
    shm->key = key;
    shm->base_addr = (void*)(uintptr_t)base_addr;  // Convert physical address to pointer
    shm->size = size;
    shm->flags = flags;
    shm->ref_count = 0;

    // Get current process
    process_t* proc = process_current();
    shm->creator_pid = proc ? proc->pid : 0;

    return shm_id;
}

// Attach to shared memory segment
void* shm_attach(uint32_t shm_id) {
    if (shm_id >= MAX_SHARED_MEMORY || !shm_segments[shm_id].in_use) {
        return NULL;  // Invalid segment
    }

    shm_segment_t* shm = &shm_segments[shm_id];

    // Increment reference count
    shm->ref_count++;

    return shm->base_addr;
}

// Detach from shared memory segment
int shm_detach(void* addr) {
    // Find segment by address
    for (uint32_t i = 0; i < MAX_SHARED_MEMORY; i++) {
        if (shm_segments[i].in_use && shm_segments[i].base_addr == addr) {
            if (shm_segments[i].ref_count > 0) {
                shm_segments[i].ref_count--;
            }
            return 0;
        }
    }

    return -1;  // Segment not found
}

// Destroy shared memory segment
int shm_destroy(uint32_t shm_id) {
    if (shm_id >= MAX_SHARED_MEMORY || !shm_segments[shm_id].in_use) {
        return -1;  // Invalid segment
    }

    shm_segment_t* shm = &shm_segments[shm_id];

    // Only destroy if no references
    if (shm->ref_count > 0) {
        return -1;  // Still in use
    }

    // Free physical pages
    // (Simplified - should free all allocated pages for the segment)
    if (shm->base_addr) {
        extern void pmm_free_page(uint32_t addr);
        pmm_free_page((uint32_t)(uintptr_t)shm->base_addr);
    }

    // Clear segment
    shm->in_use = 0;
    shm->key = 0;
    shm->base_addr = NULL;
    shm->size = 0;
    shm->ref_count = 0;
    shm->creator_pid = 0;

    return 0;
}

// ============================================================================
// MESSAGE QUEUE OPERATIONS
// ============================================================================

// Create message queue
uint32_t msgq_create(uint32_t key, uint32_t flags) {
    // Check if queue with this key already exists
    for (uint32_t i = 0; i < MAX_MESSAGE_QUEUES; i++) {
        if (msg_queues[i].in_use && msg_queues[i].key == key) {
            return i;  // Return existing queue
        }
    }

    // Find free queue slot
    uint32_t msgq_id = 0;
    for (msgq_id = 0; msgq_id < MAX_MESSAGE_QUEUES; msgq_id++) {
        if (!msg_queues[msgq_id].in_use) {
            break;
        }
    }

    if (msgq_id >= MAX_MESSAGE_QUEUES) {
        return (uint32_t)-1;  // No free queues
    }

    // Initialize queue
    msg_queue_t* msgq = &msg_queues[msgq_id];
    msgq->in_use = 1;
    msgq->key = key;
    msgq->count = 0;
    msgq->head = 0;
    msgq->tail = 0;
    msgq->flags = flags;

    return msgq_id;
}

// Send message to queue
int msgq_send(uint32_t msgq_id, const void* msg, uint32_t size, uint32_t type) {
    if (msgq_id >= MAX_MESSAGE_QUEUES || !msg_queues[msgq_id].in_use) {
        return -1;  // Invalid queue
    }

    if (size > MSG_MAX_SIZE) {
        return -1;  // Message too large
    }

    msg_queue_t* msgq = &msg_queues[msgq_id];

    // Check if queue is full
    if (msgq->count >= MSG_QUEUE_SIZE) {
        return -1;  // Queue full
    }

    // Get current process
    process_t* proc = process_current();
    uint32_t sender_pid = proc ? proc->pid : 0;

    // Add message to queue
    message_t* message = &msgq->messages[msgq->tail];
    message->type = type;
    message->sender_pid = sender_pid;
    message->size = size;
    mem_copy(message->data, msg, size);

    // Update queue pointers
    msgq->tail = (msgq->tail + 1) % MSG_QUEUE_SIZE;
    msgq->count++;

    return 0;
}

// Receive message from queue
int msgq_receive(uint32_t msgq_id, void* msg, uint32_t size, uint32_t* type) {
    if (msgq_id >= MAX_MESSAGE_QUEUES || !msg_queues[msgq_id].in_use) {
        return -1;  // Invalid queue
    }

    msg_queue_t* msgq = &msg_queues[msgq_id];

    // Check if queue is empty
    if (msgq->count == 0) {
        return -1;  // No messages
    }

    // Get message from queue
    message_t* message = &msgq->messages[msgq->head];

    // Check buffer size
    if (size < message->size) {
        return -1;  // Buffer too small
    }

    // Copy message data
    mem_copy(msg, message->data, message->size);
    if (type) {
        *type = message->type;
    }

    uint32_t msg_size = message->size;

    // Update queue pointers
    msgq->head = (msgq->head + 1) % MSG_QUEUE_SIZE;
    msgq->count--;

    return (int)msg_size;
}

// Destroy message queue
int msgq_destroy(uint32_t msgq_id) {
    if (msgq_id >= MAX_MESSAGE_QUEUES || !msg_queues[msgq_id].in_use) {
        return -1;  // Invalid queue
    }

    // Clear queue
    msg_queues[msgq_id].in_use = 0;
    msg_queues[msgq_id].key = 0;
    msg_queues[msgq_id].count = 0;
    msg_queues[msgq_id].head = 0;
    msg_queues[msgq_id].tail = 0;

    return 0;
}

// ============================================================================
// SIGNAL OPERATIONS
// ============================================================================

// Set signal handler for a process
int signal_set_handler(uint32_t pid, int signum, signal_handler_t handler) {
    if (pid >= MAX_PROCESSES || signum < 1 || signum >= (int)MAX_SIGNALS) {
        return -1;  // Invalid PID or signal number
    }

    signal_table[pid].handlers[signum] = handler;
    return 0;
}

// Send signal to a process
int signal_send(uint32_t pid, int signum) {
    if (pid >= MAX_PROCESSES || signum < 1 || signum >= (int)MAX_SIGNALS) {
        return -1;  // Invalid PID or signal number
    }

    // Check if signal is blocked
    if (signal_table[pid].blocked & (1 << signum)) {
        // Signal is blocked, add to pending
        signal_table[pid].pending |= (1 << signum);
        return 0;
    }

    // Set pending bit
    signal_table[pid].pending |= (1 << signum);

    // Note: Actual signal dispatch would happen during context switch
    // or at next opportunity when process is scheduled

    return 0;
}

// Block a signal
int signal_block(uint32_t pid, int signum) {
    if (pid >= MAX_PROCESSES || signum < 1 || signum >= (int)MAX_SIGNALS) {
        return -1;  // Invalid PID or signal number
    }

    // SIGKILL and SIGSTOP cannot be blocked
    if (signum == SIGKILL || signum == SIGSTOP) {
        return -1;
    }

    signal_table[pid].blocked |= (1 << signum);
    return 0;
}

// Unblock a signal
int signal_unblock(uint32_t pid, int signum) {
    if (pid >= MAX_PROCESSES || signum < 1 || signum >= (int)MAX_SIGNALS) {
        return -1;  // Invalid PID or signal number
    }

    signal_table[pid].blocked &= ~(1 << signum);

    // Check if signal was pending
    if (signal_table[pid].pending & (1 << signum)) {
        signal_dispatch(pid);
    }

    return 0;
}

// Dispatch pending signals for a process
void signal_dispatch(uint32_t pid) {
    if (pid >= MAX_PROCESSES) {
        return;  // Invalid PID
    }

    signal_info_t* sig_info = &signal_table[pid];

    // Check each signal
    for (int signum = 1; signum < (int)MAX_SIGNALS; signum++) {
        // Check if signal is pending and not blocked
        if ((sig_info->pending & (1 << signum)) &&
            !(sig_info->blocked & (1 << signum))) {

            // Clear pending bit
            sig_info->pending &= ~(1 << signum);

            // Get handler
            signal_handler_t handler = sig_info->handlers[signum];

            if (handler == (signal_handler_t)SIG_IGN) {
                // Ignore signal
                continue;
            } else if (handler == (signal_handler_t)SIG_DFL) {
                // Default action (simplified)
                // Most signals terminate the process
                if (signum == SIGKILL || signum == SIGTERM || signum == SIGINT) {
                    printf("[IPC] Process %d received signal %d (terminating)\n", pid, signum);
                    // Would terminate process here
                } else if (signum == SIGCHLD) {
                    // Ignore by default
                } else if (signum == SIGSTOP) {
                    printf("[IPC] Process %d received SIGSTOP\n", pid);
                    // Would stop process here
                }
            } else {
                // Custom handler
                // In a real implementation, this would require switching to
                // user mode and calling the handler function
                printf("[IPC] Process %d: calling signal handler for signal %d\n", pid, signum);
                // handler(signum);  // Can't call directly from kernel
            }
        }
    }
}
