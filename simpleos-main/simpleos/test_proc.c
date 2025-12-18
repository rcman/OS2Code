// File: test_proc.c
// Test processes for multi-tasking

#include "types.h"

// External functions
extern void printf(const char* format, ...);

// DosExit syscall
extern void DosExit(uint32_t exit_code);

// Test process 1 - Prints 'A' (runs forever)
void test_process_1(void) {
    int counter = 0;
    while (1) {
        printf("A");
        counter++;
        if (counter % 10 == 0) {
            printf(" ");
        }

        // Busy loop to slow down output
        for (volatile int i = 0; i < 1000000; i++) {}
    }
}

// Test process 4 - Prints 'X' then exits after 5 iterations
void test_process_4(void) {
    printf("\n[TestX] Starting finite process...\n");
    for (int counter = 0; counter < 5; counter++) {
        printf("X");
        // Busy loop
        for (volatile int i = 0; i < 1000000; i++) {}
    }
    printf("\n[TestX] Exiting gracefully\n");
    DosExit(0);
    // Should never reach here
    printf("[TestX] ERROR: Returned from DosExit!\n");
}

// Test process 2 - Prints 'B'
void test_process_2(void) {
    int counter = 0;
    while (1) {
        printf("B");
        counter++;
        if (counter % 10 == 0) {
            printf(" ");
        }

        // Busy loop to slow down output
        for (volatile int i = 0; i < 1000000; i++) {}
    }
}

// Test process 3 - Prints 'C'
void test_process_3(void) {
    int counter = 0;
    while (1) {
        printf("C");
        counter++;
        if (counter % 10 == 0) {
            printf(" ");
        }

        // Busy loop to slow down output
        for (volatile int i = 0; i < 1000000; i++) {}
    }
}

// Test process 5 - Parent that creates children
extern uint32_t process_create(const char* name, void (*entry_point)(void), uint32_t priority);
extern void scheduler_start(void);

void test_child_process(void) {
    printf("\n[Child] I am a child process, running briefly...\n");
    for (int i = 0; i < 3; i++) {
        printf("c");
        for (volatile int j = 0; j < 1000000; j++) {}
    }
    printf("\n[Child] Exiting...\n");
    DosExit(42);  // Exit with code 42
}

void test_parent_process(void) {
    printf("\n[Parent] I am the parent process\n");
    printf("[Parent] Creating 2 child processes...\n");

    // Note: This won't work perfectly as we need kernel privileges to create processes
    // But we'll demonstrate the hierarchy when created from kernel

    printf("[Parent] Running for a while...\n");
    for (int i = 0; i < 10; i++) {
        printf("P");
        for (volatile int j = 0; j < 1000000; j++) {}
    }

    printf("\n[Parent] Exiting (children will be orphaned)\n");
    DosExit(0);
}
