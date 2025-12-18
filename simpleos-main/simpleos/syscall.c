// File: syscall.c
// System call handler

#include "types.h"

// External functions
extern void printf(const char* format, ...);
extern void vga_putchar(char c);
extern void vga_puts(const char* str);
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void process_exit(uint32_t pid);
extern void process_exit_with_code(uint32_t pid, uint32_t exit_code);
extern void* process_current(void);
extern uint32_t process_get_pid(void* proc);
extern void scheduler_schedule(void);

// Syscall numbers
#define SYSCALL_EXIT    1
#define SYSCALL_WRITE   2
#define SYSCALL_READ    3
#define SYSCALL_FORK    4
#define SYSCALL_EXEC    5
#define SYSCALL_GETPID  6

// User mode test handler
extern void usermode_syscall_handler(registers_t* regs);

// Syscall handler
static void syscall_handler(registers_t* regs) {
    uint32_t syscall_num = regs->eax;

    // Check if this is the user mode test syscall
    if (syscall_num == 0x01) {
        usermode_syscall_handler(regs);
        return;
    }

    switch (syscall_num) {
        case SYSCALL_EXIT: {
            uint32_t exit_code = regs->ebx;
            void* proc = process_current();
            if (proc) {
                uint32_t pid = process_get_pid(proc);
                printf("[Syscall] Process %d exiting with code %d\n", pid, exit_code);
                process_exit_with_code(pid, exit_code);
                // Schedule next process (current process is now terminated)
                scheduler_schedule();
            }
            // Should never reach here as scheduler switched to another process
            break;
        }
        
        case SYSCALL_WRITE: {
            // Args: EBX = fd, ECX = buffer, EDX = length
            uint32_t fd = regs->ebx;
            const char* buffer = (const char*)regs->ecx;
            uint32_t length = regs->edx;

            if (fd == 1 || fd == 2) {  // stdout or stderr
                // Write characters to VGA
                for (uint32_t i = 0; i < length; i++) {
                    char c = buffer[i];
                    vga_putchar(c);
                }
                regs->eax = length;
            } else {
                regs->eax = (uint32_t)-1;  // Error
            }
            break;
        }
        
        case SYSCALL_READ: {
            // Not implemented yet
            regs->eax = (uint32_t)-1;
            break;
        }
        
        case SYSCALL_FORK: {
            printf("[Syscall] FORK called (not implemented)\n");
            regs->eax = (uint32_t)-1;
            break;
        }
        
        case SYSCALL_EXEC: {
            printf("[Syscall] EXEC called (not implemented)\n");
            regs->eax = (uint32_t)-1;
            break;
        }
        
        case SYSCALL_GETPID: {
            // Return a dummy PID
            regs->eax = 1;
            break;
        }
        
        default:
            printf("[Syscall] Unknown syscall %d\n", syscall_num);
            regs->eax = (uint32_t)-1;
            break;
    }
}

// Initialize syscall handler
void syscall_init(void) {
    register_interrupt_handler(128, syscall_handler);
    printf("[Syscall] Handler registered (int 0x80)\n");
}
