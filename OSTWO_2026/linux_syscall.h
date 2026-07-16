// File: linux_syscall.h
// Linux syscall compatibility layer

#ifndef LINUX_SYSCALL_H
#define LINUX_SYSCALL_H

#include "types.h"

// Linux syscall numbers (i386)
#define LINUX_SYS_exit          1
#define LINUX_SYS_fork          2
#define LINUX_SYS_read          3
#define LINUX_SYS_write         4
#define LINUX_SYS_open          5
#define LINUX_SYS_close         6
#define LINUX_SYS_waitpid       7
#define LINUX_SYS_creat         8
#define LINUX_SYS_link          9
#define LINUX_SYS_unlink        10
#define LINUX_SYS_execve        11
#define LINUX_SYS_chdir         12
#define LINUX_SYS_time          13
#define LINUX_SYS_getpid        20
#define LINUX_SYS_getuid        24
#define LINUX_SYS_access        33
#define LINUX_SYS_kill          37
#define LINUX_SYS_brk           45
#define LINUX_SYS_getgid        47
#define LINUX_SYS_geteuid       49
#define LINUX_SYS_getegid       50
#define LINUX_SYS_ioctl         54
#define LINUX_SYS_fcntl         55
#define LINUX_SYS_dup2          63
#define LINUX_SYS_getppid       64
#define LINUX_SYS_setsid        66
#define LINUX_SYS_sigaction     67
#define LINUX_SYS_setreuid      70
#define LINUX_SYS_setregid      71
#define LINUX_SYS_munmap        91
#define LINUX_SYS_fstat         108
#define LINUX_SYS_clone         120
#define LINUX_SYS_mprotect      125
#define LINUX_SYS_rt_sigaction  174
#define LINUX_SYS_rt_sigprocmask 175
#define LINUX_SYS_getcwd        183
#define LINUX_SYS_mmap2         192
#define LINUX_SYS_stat64        195
#define LINUX_SYS_fstat64       197
#define LINUX_SYS_getuid32      199
#define LINUX_SYS_getgid32      200
#define LINUX_SYS_geteuid32     201
#define LINUX_SYS_getegid32     202
#define LINUX_SYS_exit_group    252

// Initialize Linux syscall handler
void linux_syscall_init(void);

// Linux syscall handler (called from int 0x80)
void linux_syscall_handler(registers_t* regs);

#endif // LINUX_SYSCALL_H
