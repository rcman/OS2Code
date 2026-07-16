// File: bga.h
// Bochs Graphics Adapter (BGA / Bochs VBE DISPI) driver
//
// QEMU's "std" VGA and Bochs expose the DISPI interface: a linear
// framebuffer whose resolution can be programmed from protected mode
// through I/O ports 0x1CE/0x1CF, with no BIOS calls. This lets the
// kernel reach high-resolution 32-bit modes even when the bootloader
// (e.g. QEMU's -kernel multiboot loader) did not set up a framebuffer.

#ifndef BGA_H
#define BGA_H

#include "types.h"

// DISPI I/O ports
#define VBE_DISPI_IOPORT_INDEX  0x01CE
#define VBE_DISPI_IOPORT_DATA   0x01CF

// DISPI register indices
#define VBE_DISPI_INDEX_ID          0
#define VBE_DISPI_INDEX_XRES        1
#define VBE_DISPI_INDEX_YRES        2
#define VBE_DISPI_INDEX_BPP         3
#define VBE_DISPI_INDEX_ENABLE      4
#define VBE_DISPI_INDEX_BANK        5
#define VBE_DISPI_INDEX_VIRT_WIDTH  6
#define VBE_DISPI_INDEX_VIRT_HEIGHT 7
#define VBE_DISPI_INDEX_X_OFFSET    8
#define VBE_DISPI_INDEX_Y_OFFSET    9

// DISPI ID values (0xB0C0 = original, 0xB0C5 = latest)
#define VBE_DISPI_ID0           0xB0C0
#define VBE_DISPI_ID5           0xB0C5

// DISPI enable register bits
#define VBE_DISPI_DISABLED      0x00
#define VBE_DISPI_ENABLED       0x01
#define VBE_DISPI_LFB_ENABLED   0x40
#define VBE_DISPI_NOCLEARMEM    0x80

// Detect the BGA/DISPI interface and program (but not activate) the
// given video mode. On success, fills in the global vbe_mode_info
// (vbe.h) exactly as if the bootloader had provided the framebuffer,
// and returns 0. The display stays in text mode until bga_activate()
// so boot messages remain visible.
// Returns -1 if no BGA hardware is present or the framebuffer BAR
// could not be located.
int bga_init(uint16_t width, uint16_t height, uint8_t bpp);

// Switch the display to the mode programmed by bga_init().
// Safe no-op if bga_init() was never called or failed (e.g. when the
// bootloader already provided a framebuffer).
void bga_activate(void);

// Returns 1 if BGA hardware was detected (valid DISPI ID), else 0.
int bga_available(void);

#endif // BGA_H
