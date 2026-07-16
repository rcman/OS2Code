// File: serial.h
// Serial port driver (COM1/COM2) - RS-232 UART (16550)

#ifndef SERIAL_H
#define SERIAL_H

#include "types.h"

// Serial port base addresses
#define SERIAL_COM1     0x3F8
#define SERIAL_COM2     0x2F8
#define SERIAL_COM3     0x3E8
#define SERIAL_COM4     0x2E8

// UART register offsets
#define SERIAL_DATA         0   // Data register (DLAB=0)
#define SERIAL_DIVISOR_LSB  0   // Divisor latch LSB (DLAB=1)
#define SERIAL_INT_ENABLE   1   // Interrupt enable register (DLAB=0)
#define SERIAL_DIVISOR_MSB  1   // Divisor latch MSB (DLAB=1)
#define SERIAL_INT_ID       2   // Interrupt identification register
#define SERIAL_FIFO_CTRL    2   // FIFO control register
#define SERIAL_LINE_CTRL    3   // Line control register
#define SERIAL_MODEM_CTRL   4   // Modem control register
#define SERIAL_LINE_STATUS  5   // Line status register
#define SERIAL_MODEM_STATUS 6   // Modem status register
#define SERIAL_SCRATCH      7   // Scratch register

// Line control register bits
#define SERIAL_LCR_5BITS    0x00    // 5 data bits
#define SERIAL_LCR_6BITS    0x01    // 6 data bits
#define SERIAL_LCR_7BITS    0x02    // 7 data bits
#define SERIAL_LCR_8BITS    0x03    // 8 data bits
#define SERIAL_LCR_1STOP    0x00    // 1 stop bit
#define SERIAL_LCR_2STOP    0x04    // 2 stop bits
#define SERIAL_LCR_PARITY   0x08    // Parity enable
#define SERIAL_LCR_EVEN     0x10    // Even parity
#define SERIAL_LCR_STICK    0x20    // Stick parity
#define SERIAL_LCR_BREAK    0x40    // Break signal
#define SERIAL_LCR_DLAB     0x80    // Divisor latch access bit

// Line status register bits
#define SERIAL_LSR_DATA_READY       0x01    // Data ready
#define SERIAL_LSR_OVERRUN_ERROR    0x02    // Overrun error
#define SERIAL_LSR_PARITY_ERROR     0x04    // Parity error
#define SERIAL_LSR_FRAMING_ERROR    0x08    // Framing error
#define SERIAL_LSR_BREAK_INDICATOR  0x10    // Break interrupt
#define SERIAL_LSR_TRANSMIT_EMPTY   0x20    // Transmitter holding register empty
#define SERIAL_LSR_TRANSMIT_IDLE    0x40    // Transmitter empty
#define SERIAL_LSR_FIFO_ERROR       0x80    // FIFO error

// FIFO control register bits
#define SERIAL_FCR_ENABLE       0x01    // Enable FIFO
#define SERIAL_FCR_CLEAR_RX     0x02    // Clear receive FIFO
#define SERIAL_FCR_CLEAR_TX     0x04    // Clear transmit FIFO
#define SERIAL_FCR_DMA_MODE     0x08    // DMA mode select
#define SERIAL_FCR_TRIGGER_1    0x00    // Trigger level 1 byte
#define SERIAL_FCR_TRIGGER_4    0x40    // Trigger level 4 bytes
#define SERIAL_FCR_TRIGGER_8    0x80    // Trigger level 8 bytes
#define SERIAL_FCR_TRIGGER_14   0xC0    // Trigger level 14 bytes

// Baud rate divisors (for 115200 base rate)
#define SERIAL_BAUD_115200  1
#define SERIAL_BAUD_57600   2
#define SERIAL_BAUD_38400   3
#define SERIAL_BAUD_19200   6
#define SERIAL_BAUD_9600    12
#define SERIAL_BAUD_4800    24
#define SERIAL_BAUD_2400    48
#define SERIAL_BAUD_1200    96

// Initialize serial port
// port: SERIAL_COM1, SERIAL_COM2, etc.
// baud: Baud rate divisor (e.g., SERIAL_BAUD_9600)
void serial_init(uint16_t port, uint16_t baud);

// Write a byte to serial port
void serial_write_byte(uint16_t port, uint8_t data);

// Write a string to serial port
void serial_write_string(uint16_t port, const char* str);

// Read a byte from serial port (blocking)
uint8_t serial_read_byte(uint16_t port);

// Check if data is available to read
int serial_has_data(uint16_t port);

// Check if transmit buffer is empty
int serial_can_write(uint16_t port);

// Print formatted string to serial port
void serial_printf(uint16_t port, const char* format, ...);

#endif // SERIAL_H
