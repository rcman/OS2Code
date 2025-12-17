#include "../drivers/vga.h"

VGA vga;

// I/O port functions
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

void VGA::init() {
    video_memory = (uint16_t*)0xB8000;
    cursor_x = 0;
    cursor_y = 0;
    current_color = makeColor(VGA_LIGHT_GREY, VGA_BLACK);
    clear();
}

void VGA::clear() {
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            video_memory[y * WIDTH + x] = makeEntry(' ', current_color);
        }
    }
    cursor_x = 0;
    cursor_y = 0;
    updateCursor();
}

uint8_t VGA::makeColor(VGAColor fg, VGAColor bg) {
    return fg | (bg << 4);
}

uint16_t VGA::makeEntry(char c, uint8_t color) {
    return (uint16_t)c | ((uint16_t)color << 8);
}

void VGA::setColor(VGAColor fg, VGAColor bg) {
    current_color = makeColor(fg, bg);
}

void VGA::scroll() {
    // Move all lines up by one
    for (int y = 0; y < HEIGHT - 1; y++) {
        for (int x = 0; x < WIDTH; x++) {
            video_memory[y * WIDTH + x] = video_memory[(y + 1) * WIDTH + x];
        }
    }
    // Clear the last line
    for (int x = 0; x < WIDTH; x++) {
        video_memory[(HEIGHT - 1) * WIDTH + x] = makeEntry(' ', current_color);
    }
    cursor_y = HEIGHT - 1;
}

void VGA::putchar(char c) {
    if (c == '\n') {
        cursor_x = 0;
        cursor_y++;
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        cursor_x = (cursor_x + 8) & ~7;
    } else if (c == '\b') {
        backspace();
        return;
    } else {
        video_memory[cursor_y * WIDTH + cursor_x] = makeEntry(c, current_color);
        cursor_x++;
    }

    if (cursor_x >= WIDTH) {
        cursor_x = 0;
        cursor_y++;
    }

    if (cursor_y >= HEIGHT) {
        scroll();
    }

    updateCursor();
}

void VGA::backspace() {
    if (cursor_x > 0) {
        cursor_x--;
        video_memory[cursor_y * WIDTH + cursor_x] = makeEntry(' ', current_color);
    } else if (cursor_y > 0) {
        cursor_y--;
        cursor_x = WIDTH - 1;
        video_memory[cursor_y * WIDTH + cursor_x] = makeEntry(' ', current_color);
    }
    updateCursor();
}

void VGA::puts(const char* str) {
    while (*str) {
        putchar(*str++);
    }
}

void VGA::setCursor(int x, int y) {
    cursor_x = x;
    cursor_y = y;
    updateCursor();
}

void VGA::updateCursor() {
    uint16_t pos = cursor_y * WIDTH + cursor_x;
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}
