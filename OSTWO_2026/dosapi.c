// File: dosapi.c
// OS/2 DOS API implementation (syscall wrappers)

#include "dosapi.h"

// DosWrite - Write to file handle
int32_t DosWrite(uint32_t handle, const void* buffer, uint32_t length) {
    int32_t result;
    __asm__ volatile(
        "int $0x80"
        : "=a"(result)
        : "a"(SYSCALL_WRITE), "b"(handle), "c"(buffer), "d"(length)
        : "memory"
    );
    return result;
}

// DosPutChar - Write single character
void DosPutChar(char c) {
    DosWrite(STDOUT_HANDLE, &c, 1);
}

// DosPutString - Write null-terminated string
void DosPutString(const char* str) {
    uint32_t len = 0;
    while (str[len]) len++;
    DosWrite(STDOUT_HANDLE, str, len);
}

// DosExit - Terminate process
void DosExit(uint32_t exit_code) {
    __asm__ volatile(
        "int $0x80"
        :
        : "a"(SYSCALL_EXIT), "b"(exit_code)
        : "memory"
    );
    while (1) __asm__ volatile("hlt");
}

// DosGetPID - Get process ID
uint32_t DosGetPID(void) {
    uint32_t pid;
    __asm__ volatile(
        "int $0x80"
        : "=a"(pid)
        : "a"(SYSCALL_GETPID)
        : "memory"
    );
    return pid;
}
