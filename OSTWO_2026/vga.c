// File: vga.c
// VGA text mode driver implementation

#include "vga.h"

// VGA state
static uint16_t* vga_buffer = (uint16_t*)VGA_MEMORY;
static int vga_row = 0;
static int vga_col = 0;
static uint8_t vga_color_attr = 0x0F;  // White on black

// External port I/O
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

// Create VGA entry
static inline uint16_t vga_entry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

// Create color attribute
static inline uint8_t vga_color(enum vga_color fg, enum vga_color bg) {
    return fg | (bg << 4);
}

// Update hardware cursor
static void update_cursor(void) {
    uint16_t pos = vga_row * VGA_WIDTH + vga_col;
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

// Scroll screen up one line
static void scroll(void) {
    if (vga_row >= VGA_HEIGHT) {
        // Move all lines up
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_buffer[i + VGA_WIDTH];
        }
        
        // Clear last line
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga_buffer[i] = vga_entry(' ', vga_color_attr);
        }
        
        vga_row = VGA_HEIGHT - 1;
    }
}

// Initialize VGA
void vga_init(void) {
    vga_color_attr = vga_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    vga_clear();
    
    // Enable cursor
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);
    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15);
}

// Clear screen
void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga_buffer[i] = vga_entry(' ', vga_color_attr);
    }
    vga_row = 0;
    vga_col = 0;
    update_cursor();
}

// Put a character
void vga_putchar(char c) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
    } else if (c == '\r') {
        vga_col = 0;
    } else if (c == '\t') {
        vga_col = (vga_col + 8) & ~7;
    } else if (c == '\b') {
        if (vga_col > 0) {
            vga_col--;
            vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(' ', vga_color_attr);
        }
    } else {
        vga_buffer[vga_row * VGA_WIDTH + vga_col] = vga_entry(c, vga_color_attr);
        vga_col++;
    }
    
    if (vga_col >= VGA_WIDTH) {
        vga_col = 0;
        vga_row++;
    }
    
    scroll();
    update_cursor();
}

// Put a string
void vga_puts(const char* str) {
    while (*str) {
        vga_putchar(*str++);
    }
}

// Set text color
void vga_set_color(uint8_t fg, uint8_t bg) {
    vga_color_attr = vga_color(fg, bg);
}

// Set cursor position
void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_col = x;
        vga_row = y;
        update_cursor();
    }
}
