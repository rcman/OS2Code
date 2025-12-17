#ifndef VGA_H
#define VGA_H

#include "../kernel/types.h"

// VGA text mode colors
enum VGAColor {
    VGA_BLACK = 0,
    VGA_BLUE = 1,
    VGA_GREEN = 2,
    VGA_CYAN = 3,
    VGA_RED = 4,
    VGA_MAGENTA = 5,
    VGA_BROWN = 6,
    VGA_LIGHT_GREY = 7,
    VGA_DARK_GREY = 8,
    VGA_LIGHT_BLUE = 9,
    VGA_LIGHT_GREEN = 10,
    VGA_LIGHT_CYAN = 11,
    VGA_LIGHT_RED = 12,
    VGA_LIGHT_MAGENTA = 13,
    VGA_YELLOW = 14,
    VGA_WHITE = 15
};

class VGA {
public:
    static const int WIDTH = 80;
    static const int HEIGHT = 25;

    void init();
    void clear();
    void putchar(char c);
    void puts(const char* str);
    void setColor(VGAColor fg, VGAColor bg);
    void setCursor(int x, int y);
    void updateCursor();
    void scroll();
    void backspace();
    
    int getX() const { return cursor_x; }
    int getY() const { return cursor_y; }

private:
    uint16_t* video_memory;
    int cursor_x;
    int cursor_y;
    uint8_t current_color;

    uint16_t makeEntry(char c, uint8_t color);
    uint8_t makeColor(VGAColor fg, VGAColor bg);
};

extern VGA vga;

#endif
