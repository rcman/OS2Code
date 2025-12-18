// File: usermode.h
// User mode support

#ifndef USERMODE_H
#define USERMODE_H

#include "types.h"

// Jump to user mode (Ring 3)
void enter_user_mode(void);

// System call handler for user mode testing
void usermode_syscall_handler(registers_t* regs);

#endif // USERMODE_H
