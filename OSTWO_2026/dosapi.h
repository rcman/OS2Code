// File: dosapi.h
// OS/2 Compatible DOS API for user mode programs
// Provides DosXXX functions that use system calls
// All functions are static inline so they're compiled into each process

#ifndef DOSAPI_H
#define DOSAPI_H

#include "types.h"

// DateTime structure (OS/2 compatible)
typedef struct {
    uint8_t  hours;        // 0-23
    uint8_t  minutes;      // 0-59
    uint8_t  seconds;      // 0-59
    uint8_t  hundredths;   // 0-99 (centiseconds)
    uint8_t  day;          // 1-31
    uint8_t  month;        // 1-12
    uint16_t year;         // Full year (e.g., 2025)
    int16_t  timezone;     // Timezone offset in minutes from UTC
    uint8_t  weekday;      // 0=Sunday, 1=Monday, ..., 6=Saturday
} DateTime;

// Standard file handles
#define STDOUT_HANDLE 1
#define STDERR_HANDLE 2
#define STDIN_HANDLE  0

// Syscall numbers (must match syscall.c)
#define SYSCALL_EXIT    1
#define SYSCALL_WRITE   2
#define SYSCALL_READ    3
#define SYSCALL_FORK    4
#define SYSCALL_EXEC    5
#define SYSCALL_GETPID  6
#define SYSCALL_SLEEP   7
#define SYSCALL_BEEP    8
#define SYSCALL_GETDATETIME 9
#define SYSCALL_SETDATETIME 10
#define SYSCALL_GETPPID     11
#define SYSCALL_KILL        12
#define SYSCALL_SETPRIORITY 13
#define SYSCALL_WAITCHILD   14
#define SYSCALL_ALLOCMEM    15
#define SYSCALL_FREEMEM     16
#define SYSCALL_CREATEMUTEX 17
#define SYSCALL_CREATEEVENT 18
#define SYSCALL_SEMREQUEST  19
#define SYSCALL_SEMCLEAR    20
#define SYSCALL_SEMSET      21
#define SYSCALL_SEMCLOSE    22
#define SYSCALL_CREATETHREAD 23
#define SYSCALL_EXITTHREAD  24
#define SYSCALL_GETTID      25

// DosWrite - Write to file handle
// Returns number of bytes written, or -1 on error
static inline int32_t DosWrite(uint32_t handle, const void* buffer, uint32_t length) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_WRITE), "b"(handle), "c"(buffer), "d"(length)
        : "memory"
    );
    return result;
}

// DosRead - Read from file handle
// Returns number of bytes read, or -1 on error
// NOTE: Currently not implemented. Will be added in Phase 2.
static inline int32_t DosRead(uint32_t handle, void* buffer, uint32_t length) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_READ), "b"(handle), "c"(buffer), "d"(length)
        : "memory"
    );
    return result;
}

// DosPutChar - Write single character to stdout
static inline void DosPutChar(char c) {
    DosWrite(STDOUT_HANDLE, &c, 1);
}

// DosPutString - Write null-terminated string to stdout
static inline void DosPutString(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    DosWrite(STDOUT_HANDLE, str, len);
}

// DosExit - Terminate current process with exit code
// This function does not return
static inline void DosExit(uint32_t exit_code) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT), "b"(exit_code)
        : "memory"
    );
    // Should never reach here, but just in case
    while (1) __asm__ volatile("hlt");
}

// DosGetPID - Get current process ID
// Returns the PID of the calling process
static inline uint32_t DosGetPID(void) {
    uint32_t pid;
    __asm__ volatile(
        "int $0x80"
        : "=a"(pid)
        : "a"(SYSCALL_GETPID)
        : "memory"
    );
    return pid;
}

// DosExecPgm - Execute program from RamFS
// Returns child PID on success, 0 on failure
static inline uint32_t DosExecPgm(const char* filename) {
    uint32_t pid;
    __asm__ volatile(
        "int $0x80"
        : "=a"(pid)
        : "a"(SYSCALL_EXEC), "b"(filename)
        : "memory"
    );
    return pid;
}

// DosSleep - Sleep for specified milliseconds
// milliseconds: Time to sleep in milliseconds
// Returns 0 on success
// NOTE: Current implementation is busy-wait. Phase 2 will add proper blocking.
static inline uint32_t DosSleep(uint32_t milliseconds) {
    uint32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SLEEP), "b"(milliseconds)
        : "memory"
    );
    return result;
}

// DosBeep - Play a tone through the PC speaker
// frequency: Tone frequency in Hz (e.g., 1000 = 1 kHz)
// duration: Duration in milliseconds
// Returns 0 on success
static inline uint32_t DosBeep(uint32_t frequency, uint32_t duration) {
    uint32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_BEEP), "b"(frequency), "c"(duration)
        : "memory"
    );
    return result;
}

// DosGetDateTime - Get current system date and time
// dt: Pointer to DateTime structure to fill
// Returns 0 on success, -1 on error
static inline int32_t DosGetDateTime(DateTime* dt) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_GETDATETIME), "b"(dt)
        : "memory"
    );
    return result;
}

// DosSetDateTime - Set system date and time
// dt: Pointer to DateTime structure with new date/time
// Returns 0 on success, -1 on error
static inline int32_t DosSetDateTime(const DateTime* dt) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SETDATETIME), "b"(dt)
        : "memory"
    );
    return result;
}

// DosGetPPID - Get parent process ID
// Returns parent PID, or 0 if no parent
static inline uint32_t DosGetPPID(void) {
    uint32_t ppid;
    __asm__ volatile(
        "int $0x80"
        : "=a"(ppid)
        : "a"(SYSCALL_GETPPID)
        : "memory"
    );
    return ppid;
}

// DosKillProcess - Terminate a process by PID
// pid: Process ID to terminate
// Returns 0 on success, -1 on error
static inline int32_t DosKillProcess(uint32_t pid) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_KILL), "b"(pid), "c"(0)
        : "memory"
    );
    return result;
}

// DosSetPriority - Set process priority
// pid: Process ID (0 = current process)
// priority: New priority (0=IDLE, 1=REGULAR, 2=HIGH, 3=REALTIME)
// Returns 0 on success, -1 on error
static inline int32_t DosSetPriority(uint32_t pid, uint32_t priority) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SETPRIORITY), "b"(pid), "c"(priority)
        : "memory"
    );
    return result;
}

// DosAllocMem - Allocate memory for current process
// size_bytes: Number of bytes to allocate (will be rounded up to page size)
// Returns virtual address of allocated memory, or 0 on failure
static inline void* DosAllocMem(uint32_t size_bytes) {
    void* result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_ALLOCMEM), "b"(size_bytes)
        : "memory"
    );
    return result;
}

// DosFreeMem - Free previously allocated memory
// addr: Virtual address returned by DosAllocMem
// Returns 0 on success, -1 on error
static inline int32_t DosFreeMem(void* addr) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_FREEMEM), "b"(addr)
        : "memory"
    );
    return result;
}

// DosCreateMutexSem - Create a mutex semaphore
// name: Optional name for the semaphore (for debugging)
// initial_state: 1 = unlocked, 0 = locked
// Returns semaphore handle on success, 0 on failure
static inline uint32_t DosCreateMutexSem(const char* name, uint32_t initial_state) {
    uint32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_CREATEMUTEX), "b"(name), "c"(initial_state)
        : "memory"
    );
    return result;
}

// DosCreateEventSem - Create an event semaphore
// name: Optional name for the semaphore (for debugging)
// initial_state: 1 = signaled, 0 = not signaled
// Returns semaphore handle on success, 0 on failure
static inline uint32_t DosCreateEventSem(const char* name, uint32_t initial_state) {
    uint32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_CREATEEVENT), "b"(name), "c"(initial_state)
        : "memory"
    );
    return result;
}

// DosSemRequest - Request (acquire) a semaphore
// sem_id: Semaphore handle
// timeout_ms: Timeout in milliseconds (0 = no wait, -1 = wait forever)
// Returns 0 on success, -1 on error/timeout
static inline int32_t DosSemRequest(uint32_t sem_id, uint32_t timeout_ms) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SEMREQUEST), "b"(sem_id), "c"(timeout_ms)
        : "memory"
    );
    return result;
}

// DosSemClear - Clear (release) a semaphore
// sem_id: Semaphore handle
// For mutex: releases the lock
// For event: clears the event state
// Returns 0 on success, -1 on error
static inline int32_t DosSemClear(uint32_t sem_id) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SEMCLEAR), "b"(sem_id)
        : "memory"
    );
    return result;
}

// DosSemSet - Set (signal) a semaphore
// sem_id: Semaphore handle
// For event: sets the event state
// For mutex: this is an error
// Returns 0 on success, -1 on error
static inline int32_t DosSemSet(uint32_t sem_id) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SEMSET), "b"(sem_id)
        : "memory"
    );
    return result;
}

// DosCloseSem - Close (destroy) a semaphore
// sem_id: Semaphore handle
// Returns 0 on success, -1 on error
static inline int32_t DosCloseSem(uint32_t sem_id) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_SEMCLOSE), "b"(sem_id)
        : "memory"
    );
    return result;
}

// DosCreateThread - Create a new thread in the current process
// name: Thread name (for debugging)
// entry_point: Function to run in new thread
// arg: Argument to pass to thread function
// priority: Thread priority (0-3)
// Returns thread ID (TID) on success, 0 on failure
static inline uint32_t DosCreateThread(const char* name, void (*entry_point)(void* arg), void* arg, uint32_t priority) {
    uint32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_CREATETHREAD), "b"(name), "c"(entry_point), "d"(arg), "S"(priority)
        : "memory"
    );
    return result;
}

// DosExitThread - Exit the current thread
// exit_code: Thread exit code
// This function does not return
static inline void DosExitThread(uint32_t exit_code) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXITTHREAD), "b"(exit_code)
        : "memory"
    );
    // Should never reach here, but just in case
    while (1) __asm__ volatile("hlt");
}

// DosGetTID - Get current thread ID
// Returns the TID of the calling thread
static inline uint32_t DosGetTID(void) {
    uint32_t tid;
    __asm__ volatile(
        "int $0x80"
        : "=a"(tid)
        : "a"(SYSCALL_GETTID)
        : "memory"
    );
    return tid;
}

#endif // DOSAPI_H
