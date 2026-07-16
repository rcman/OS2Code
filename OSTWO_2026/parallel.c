// File: parallel.c
// Parallel port driver implementation

#include "parallel.h"

// External I/O functions
extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);

// Small delay for parallel port timing
static void parallel_delay(void) {
    // Small delay (~1 microsecond)
    for (volatile int i = 0; i < 10; i++) {
        __asm__ volatile("nop");
    }
}

// Initialize parallel port
void parallel_init(uint16_t port) {
    // Initialize printer: set INIT bit low then high
    outb(port + PARALLEL_CONTROL, 0x00);
    parallel_delay();

    // Release INIT, enable normal operation
    outb(port + PARALLEL_CONTROL, PARALLEL_CTRL_INIT | PARALLEL_CTRL_SELECT_IN);
    parallel_delay();

    printf("[Parallel] Initialized port 0x%x\n", port);
}

// Get printer status
uint8_t parallel_get_status(uint16_t port) {
    return inb(port + PARALLEL_STATUS);
}

// Check if printer is ready
int parallel_is_ready(uint16_t port) {
    uint8_t status = parallel_get_status(port);

    // Check if busy (active low, so 0 = busy)
    if (!(status & PARALLEL_STATUS_BUSY)) {
        return 0;  // Busy
    }

    // Check if error (active low, so 0 = error)
    if (!(status & PARALLEL_STATUS_ERROR)) {
        return 0;  // Error
    }

    // Check if paper out
    if (status & PARALLEL_STATUS_PAPER_OUT) {
        return 0;  // No paper
    }

    return 1;  // Ready
}

// Write a byte to parallel port
void parallel_write_byte(uint16_t port, uint8_t data) {
    // Wait for printer to be ready
    int timeout = 10000;
    while (!parallel_is_ready(port) && timeout > 0) {
        parallel_delay();
        timeout--;
    }

    if (timeout == 0) {
        printf("[Parallel] Timeout waiting for printer\n");
        return;
    }

    // Write data
    outb(port + PARALLEL_DATA, data);
    parallel_delay();

    // Toggle strobe (pulse it low then high)
    uint8_t ctrl = inb(port + PARALLEL_CONTROL);
    outb(port + PARALLEL_CONTROL, ctrl & ~PARALLEL_CTRL_STROBE);  // Strobe low
    parallel_delay();
    outb(port + PARALLEL_CONTROL, ctrl | PARALLEL_CTRL_STROBE);   // Strobe high
    parallel_delay();
}

// Write a string to parallel port
void parallel_write_string(uint16_t port, const char* str) {
    while (*str) {
        parallel_write_byte(port, *str);
        str++;
    }

    // Send form feed to print the page
    parallel_write_byte(port, 0x0C);
}
