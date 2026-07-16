// File: ipc.h
// Inter-Process Communication subsystem

#ifndef IPC_H
#define IPC_H

#include "types.h"

// Maximum IPC objects
#define MAX_PIPES           32
#define MAX_SHARED_MEMORY   32
#define MAX_MESSAGE_QUEUES  32
#define MAX_SIGNALS         32

// Pipe constants
#define PIPE_SIZE           4096    // 4KB pipe buffer
#define PIPE_BLOCK          0x01    // Blocking mode
#define PIPE_NONBLOCK       0x02    // Non-blocking mode

// Shared memory flags
#define SHM_CREATE          0x0200  // Create if doesn't exist
#define SHM_EXCL            0x0400  // Fail if exists
#define SHM_RDONLY          0x1000  // Read-only access

// Shared memory permissions
#define SHM_R               0x0100  // Read permission
#define SHM_W               0x0080  // Write permission

// Message queue constants
#define MSG_MAX_SIZE        256     // Maximum message size
#define MSG_QUEUE_SIZE      32      // Maximum messages in queue

// Message types
#define MSG_TYPE_NORMAL     0
#define MSG_TYPE_PRIORITY   1

// Signal numbers (POSIX-compatible)
#define SIGHUP      1       // Hangup
#define SIGINT      2       // Interrupt
#define SIGQUIT     3       // Quit
#define SIGILL      4       // Illegal instruction
#define SIGTRAP     5       // Trace/breakpoint trap
#define SIGABRT     6       // Abort
#define SIGBUS      7       // Bus error
#define SIGFPE      8       // Floating point exception
#define SIGKILL     9       // Kill (cannot be caught)
#define SIGUSR1     10      // User-defined signal 1
#define SIGSEGV     11      // Segmentation fault
#define SIGUSR2     12      // User-defined signal 2
#define SIGPIPE     13      // Broken pipe
#define SIGALRM     14      // Alarm clock
#define SIGTERM     15      // Termination
#define SIGCHLD     17      // Child process status change
#define SIGCONT     18      // Continue if stopped
#define SIGSTOP     19      // Stop (cannot be caught)

// Signal actions
#define SIG_DFL     0       // Default action
#define SIG_IGN     1       // Ignore signal
#define SIG_BLOCK   2       // Block signal

// Pipe structure
typedef struct {
    uint32_t id;                    // Pipe ID
    char name[64];                  // Named pipe path (empty for anonymous)
    uint8_t buffer[PIPE_SIZE];      // Circular buffer
    uint32_t read_pos;              // Read position
    uint32_t write_pos;             // Write position
    uint32_t count;                 // Bytes in buffer
    uint32_t flags;                 // Pipe flags
    uint32_t reader_pid;            // Reading process
    uint32_t writer_pid;            // Writing process
    uint32_t in_use;                // 1 if allocated
} pipe_t;

// Shared memory segment
typedef struct {
    uint32_t id;                    // Segment ID
    uint32_t key;                   // Unique key
    void* base_addr;                // Physical base address
    uint32_t size;                  // Size in bytes
    uint32_t flags;                 // Flags
    uint32_t ref_count;             // Number of attached processes
    uint32_t creator_pid;           // Creating process
    uint32_t in_use;                // 1 if allocated
} shm_segment_t;

// Message structure
typedef struct {
    uint32_t type;                  // Message type
    uint32_t sender_pid;            // Sender process ID
    uint32_t size;                  // Message size
    uint8_t data[MSG_MAX_SIZE];     // Message data
} message_t;

// Message queue structure
typedef struct {
    uint32_t id;                    // Queue ID
    uint32_t key;                   // Unique key
    message_t messages[MSG_QUEUE_SIZE]; // Message buffer
    uint32_t count;                 // Messages in queue
    uint32_t head;                  // Queue head
    uint32_t tail;                  // Queue tail
    uint32_t flags;                 // Queue flags
    uint32_t in_use;                // 1 if allocated
} msg_queue_t;

// Signal handler type
typedef void (*signal_handler_t)(int signum);

// Signal information (per process)
typedef struct {
    signal_handler_t handlers[MAX_SIGNALS]; // Signal handlers
    uint32_t pending;               // Pending signals (bitmask)
    uint32_t blocked;               // Blocked signals (bitmask)
} signal_info_t;

// Initialize IPC subsystem
void ipc_init(void);

// Pipe operations
uint32_t pipe_create(const char* name, uint32_t flags);
int pipe_read(uint32_t pipe_id, void* buffer, uint32_t size);
int pipe_write(uint32_t pipe_id, const void* buffer, uint32_t size);
int pipe_close(uint32_t pipe_id);
uint32_t pipe_open(const char* name, uint32_t flags);

// Shared memory operations
uint32_t shm_create(uint32_t key, uint32_t size, uint32_t flags);
void* shm_attach(uint32_t shm_id);
int shm_detach(void* addr);
int shm_destroy(uint32_t shm_id);

// Message queue operations
uint32_t msgq_create(uint32_t key, uint32_t flags);
int msgq_send(uint32_t msgq_id, const void* msg, uint32_t size, uint32_t type);
int msgq_receive(uint32_t msgq_id, void* msg, uint32_t size, uint32_t* type);
int msgq_destroy(uint32_t msgq_id);

// Signal operations
int signal_set_handler(uint32_t pid, int signum, signal_handler_t handler);
int signal_send(uint32_t pid, int signum);
int signal_block(uint32_t pid, int signum);
int signal_unblock(uint32_t pid, int signum);
void signal_dispatch(uint32_t pid);

#endif // IPC_H
