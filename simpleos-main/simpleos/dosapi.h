// File: dosapi.h
// OS/2 Compatible DOS API for user mode programs
// Provides DosXXX functions that use system calls

#ifndef DOSAPI_H
#define DOSAPI_H

#include "types.h"

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

// Function declarations (implemented in dosapi.c)
int32_t DosWrite(uint32_t handle, const void* buffer, uint32_t length);
void DosExit(uint32_t exit_code);
uint32_t DosGetPID(void);
void DosPutString(const char* str);
void DosPutChar(char c);

#endif // DOSAPI_H
