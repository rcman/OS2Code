// File: parallel.h
// Parallel port driver (LPT1) - Standard Printer Port

#ifndef PARALLEL_H
#define PARALLEL_H

#include "types.h"

// Parallel port base addresses
#define PARALLEL_LPT1   0x378
#define PARALLEL_LPT2   0x278
#define PARALLEL_LPT3   0x3BC

// Parallel port register offsets
#define PARALLEL_DATA       0   // Data port
#define PARALLEL_STATUS     1   // Status port
#define PARALLEL_CONTROL    2   // Control port

// Status register bits (read)
#define PARALLEL_STATUS_ERROR       0x08    // Error (active low)
#define PARALLEL_STATUS_SELECT      0x10    // Printer selected
#define PARALLEL_STATUS_PAPER_OUT   0x20    // Paper out
#define PARALLEL_STATUS_ACK         0x40    // Acknowledge
#define PARALLEL_STATUS_BUSY        0x80    // Busy (active low)

// Control register bits (write)
#define PARALLEL_CTRL_STROBE        0x01    // Strobe (active low)
#define PARALLEL_CTRL_AUTO_FEED     0x02    // Auto line feed
#define PARALLEL_CTRL_INIT          0x04    // Initialize printer (active low)
#define PARALLEL_CTRL_SELECT_IN     0x08    // Select input
#define PARALLEL_CTRL_IRQ_ENABLE    0x10    // Enable IRQ

// Initialize parallel port
void parallel_init(uint16_t port);

// Write a byte to parallel port
void parallel_write_byte(uint16_t port, uint8_t data);

// Write a string to parallel port
void parallel_write_string(uint16_t port, const char* str);

// Check if printer is ready
int parallel_is_ready(uint16_t port);

// Check printer status
uint8_t parallel_get_status(uint16_t port);

#endif // PARALLEL_H
