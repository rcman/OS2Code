// File: serial.c
// Serial port driver implementation

#include "serial.h"

// External I/O functions
extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);

// Initialize serial port
void serial_init(uint16_t port, uint16_t baud) {
    // Disable interrupts
    outb(port + SERIAL_INT_ENABLE, 0x00);

    // Enable DLAB (set baud rate divisor)
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_DLAB);

    // Set divisor (lo byte)
    outb(port + SERIAL_DIVISOR_LSB, (uint8_t)(baud & 0xFF));

    // Set divisor (hi byte)
    outb(port + SERIAL_DIVISOR_MSB, (uint8_t)((baud >> 8) & 0xFF));

    // 8 bits, no parity, 1 stop bit (disable DLAB)
    outb(port + SERIAL_LINE_CTRL, SERIAL_LCR_8BITS | SERIAL_LCR_1STOP);

    // Enable FIFO, clear them, with 14-byte threshold
    outb(port + SERIAL_FIFO_CTRL,
         SERIAL_FCR_ENABLE | SERIAL_FCR_CLEAR_RX | SERIAL_FCR_CLEAR_TX | SERIAL_FCR_TRIGGER_14);

    // Mark data terminal ready, signal request to send and enable interrupts
    outb(port + SERIAL_MODEM_CTRL, 0x0B);

    // Test serial chip (loopback test)
    outb(port + SERIAL_MODEM_CTRL, 0x1E);  // Set in loopback mode, test the serial chip
    outb(port + SERIAL_DATA, 0xAE);        // Send test byte 0xAE

    // Check if serial is faulty (i.e., not same byte as sent)
    if (inb(port + SERIAL_DATA) != 0xAE) {
        printf("[Serial] ERROR: Port 0x%x is faulty\n", port);
        return;
    }

    // If serial is not faulty, set it in normal operation mode
    // (not-loopback with IRQs enabled and OUT#1 and OUT#2 bits enabled)
    outb(port + SERIAL_MODEM_CTRL, 0x0F);

    printf("[Serial] Initialized port 0x%x at baud divisor %d\n", port, baud);
}

// Check if transmit buffer is empty
int serial_can_write(uint16_t port) {
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_TRANSMIT_EMPTY;
}

// Write a byte to serial port
void serial_write_byte(uint16_t port, uint8_t data) {
    // Wait for transmit buffer to be empty
    while (!serial_can_write(port)) {
        // Busy wait
    }

    outb(port + SERIAL_DATA, data);
}

// Write a string to serial port
void serial_write_string(uint16_t port, const char* str) {
    while (*str) {
        serial_write_byte(port, *str);
        str++;
    }
}

// Check if data is available to read
int serial_has_data(uint16_t port) {
    return inb(port + SERIAL_LINE_STATUS) & SERIAL_LSR_DATA_READY;
}

// Read a byte from serial port (blocking)
uint8_t serial_read_byte(uint16_t port) {
    // Wait for data to be available
    while (!serial_has_data(port)) {
        // Busy wait
    }

    return inb(port + SERIAL_DATA);
}

// Print formatted string to serial port (simplified version)
void serial_printf(uint16_t port, const char* format, ...) {
    // For now, just print the format string
    // A full implementation would use va_list like printf
    serial_write_string(port, format);
}
