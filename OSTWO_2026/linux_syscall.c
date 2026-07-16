// File: linux_syscall.c
// Linux syscall compatibility layer implementation

#include "linux_syscall.h"
#include "process.h"

// External functions
extern void printf(const char* format, ...);
extern void process_exit_with_code(uint32_t pid, uint32_t exit_code);
extern process_t* process_current(void);
extern uint32_t process_get_pid(process_t* proc);
extern uint32_t process_get_ppid(process_t* proc);
extern void vga_putchar(char c);
extern uint32_t timer_get_ticks(void);

// Linux syscall handler
void linux_syscall_handler(registers_t* regs) {
    uint32_t syscall_num = regs->eax;

    // Linux syscalls pass arguments in: EBX, ECX, EDX, ESI, EDI, EBP
    uint32_t arg1 = regs->ebx;
    uint32_t arg2 = regs->ecx;
    uint32_t arg3 = regs->edx;
    // uint32_t arg4 = regs->esi;
    // uint32_t arg5 = regs->edi;
    // uint32_t arg6 = regs->ebp;

    // Debug logging for first few syscalls
    static int linux_syscall_count = 0;
    if (linux_syscall_count < 10) {
        printf("[Linux-Syscall] #%d: syscall=%d, args=(%d, %d, %d)\n",
               linux_syscall_count, syscall_num, arg1, arg2, arg3);
        linux_syscall_count++;
    }

    switch (syscall_num) {
        case LINUX_SYS_exit:
        case LINUX_SYS_exit_group: {
            // exit(int status)
            uint32_t exit_code = arg1;
            process_t* proc = process_current();
            if (proc) {
                uint32_t pid = process_get_pid(proc);
                printf("\n[Linux-Syscall] Process %d exiting with code %d\n", pid, exit_code);
                process_exit_with_code(pid, exit_code);

                // Return to kernel
                extern void exit_to_kernel(void);
                exit_to_kernel();
            }
            break;
        }

        case LINUX_SYS_write: {
            // write(int fd, const void *buf, size_t count)
            uint32_t fd = arg1;
            const char* buffer = (const char*)arg2;
            uint32_t length = arg3;

            if (fd == 1 || fd == 2) {  // stdout or stderr
                for (uint32_t i = 0; i < length; i++) {
                    vga_putchar(buffer[i]);
                }
                regs->eax = length;  // Return bytes written
            } else {
                // TODO: Use VFS for file I/O
                regs->eax = (uint32_t)-1;  // Error
            }
            break;
        }

        case LINUX_SYS_read: {
            // read(int fd, void *buf, size_t count)
            // TODO: Implement proper read
            regs->eax = (uint32_t)-1;  // Not implemented
            break;
        }

        case LINUX_SYS_open: {
            // open(const char *pathname, int flags, mode_t mode)
            // TODO: Use VFS for file operations
            regs->eax = (uint32_t)-1;  // Not implemented
            break;
        }

        case LINUX_SYS_close: {
            // close(int fd)
            // TODO: Use VFS for file operations
            regs->eax = (uint32_t)-1;  // Not implemented
            break;
        }

        case LINUX_SYS_getpid: {
            // getpid()
            process_t* proc = process_current();
            if (proc) {
                regs->eax = process_get_pid(proc);
            } else {
                regs->eax = 0;
            }
            break;
        }

        case LINUX_SYS_getppid: {
            // getppid()
            process_t* proc = process_current();
            if (proc) {
                regs->eax = process_get_ppid(proc);
            } else {
                regs->eax = 0;
            }
            break;
        }

        case LINUX_SYS_getuid:
        case LINUX_SYS_getuid32: {
            // getuid() - Return UID 0 (root) for now
            regs->eax = 0;
            break;
        }

        case LINUX_SYS_getgid:
        case LINUX_SYS_getgid32: {
            // getgid() - Return GID 0 (root) for now
            regs->eax = 0;
            break;
        }

        case LINUX_SYS_geteuid:
        case LINUX_SYS_geteuid32: {
            // geteuid() - Return EUID 0 (root) for now
            regs->eax = 0;
            break;
        }

        case LINUX_SYS_getegid:
        case LINUX_SYS_getegid32: {
            // getegid() - Return EGID 0 (root) for now
            regs->eax = 0;
            break;
        }

        case LINUX_SYS_brk: {
            // brk(void *addr) - Set program break (heap)
            // TODO: Implement proper heap management
            // For now, just return current brk (fake it)
            static uint32_t current_brk = 0x40000000;  // Start at 1GB
            uint32_t new_brk = arg1;

            if (new_brk == 0) {
                // Query current brk
                regs->eax = current_brk;
            } else {
                // Set new brk (TODO: actually allocate pages)
                current_brk = new_brk;
                regs->eax = current_brk;
            }
            break;
        }

        case LINUX_SYS_time: {
            // time(time_t *tloc)
            // Return seconds since epoch (fake it with ticks)
            uint32_t ticks = timer_get_ticks();
            uint32_t seconds = ticks / 100;  // 100 Hz timer

            if (arg1 != 0) {
                uint32_t* tloc = (uint32_t*)arg1;
                *tloc = seconds;
            }

            regs->eax = seconds;
            break;
        }

        case LINUX_SYS_mmap2: {
            // mmap2(void *addr, size_t length, int prot, int flags, int fd, off_t pgoffset)
            // TODO: Implement proper mmap
            regs->eax = (uint32_t)-1;  // Not implemented
            break;
        }

        case LINUX_SYS_munmap: {
            // munmap(void *addr, size_t length)
            // TODO: Implement proper munmap
            regs->eax = 0;  // Fake success
            break;
        }

        case LINUX_SYS_mprotect: {
            // mprotect(void *addr, size_t len, int prot)
            // TODO: Implement memory protection
            regs->eax = 0;  // Fake success
            break;
        }

        case LINUX_SYS_fork:
        case LINUX_SYS_clone: {
            // fork() / clone()
            printf("[Linux-Syscall] fork/clone not implemented\n");
            regs->eax = (uint32_t)-1;
            break;
        }

        case LINUX_SYS_execve: {
            // execve(const char *filename, char *const argv[], char *const envp[])
            printf("[Linux-Syscall] execve not implemented\n");
            regs->eax = (uint32_t)-1;
            break;
        }

        case LINUX_SYS_rt_sigaction:
        case LINUX_SYS_rt_sigprocmask:
        case LINUX_SYS_sigaction: {
            // Signal handling - not implemented
            regs->eax = 0;  // Fake success
            break;
        }

        default:
            printf("[Linux-Syscall] Unknown syscall %d\n", syscall_num);
            regs->eax = (uint32_t)-1;  // Return -1 (ENOSYS)
            break;
    }
}

// Initialize Linux syscall handler
void linux_syscall_init(void) {
    printf("[Linux-Syscall] Linux syscall compatibility layer initialized\n");
    // Note: The actual int 0x80 handler is already registered in syscall_init()
    // We just need to detect if the calling process is a Linux binary and
    // route to linux_syscall_handler() instead of the OS/2 syscall handler
}
