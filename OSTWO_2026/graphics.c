// File: graphics.c
// Simple framebuffer graphics (VGA 320x200 Mode 13h for reliability)
// Note: VBE high-res modes require BIOS calls before protected mode

#include "types.h"

// External functions
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);

// VGA Mode 13h (320x200x256)
#define GFX_WIDTH   320
#define GFX_HEIGHT  200
#define GFX_MEMORY  0xA0000

static uint8_t* framebuffer = (uint8_t*)GFX_MEMORY;
static bool graphics_enabled = false;

// Set VGA Mode 13h (320x200x256 colors)
// Note: This uses VGA registers, not BIOS, so it works in protected mode
void graphics_set_mode_13h(void) {
    // Miscellaneous Output Register
    outb(0x3C2, 0x63);
    
    // Sequencer registers
    outb(0x3C4, 0x00); outb(0x3C5, 0x03);
    outb(0x3C4, 0x01); outb(0x3C5, 0x01);
    outb(0x3C4, 0x02); outb(0x3C5, 0x0F);
    outb(0x3C4, 0x03); outb(0x3C5, 0x00);
    outb(0x3C4, 0x04); outb(0x3C5, 0x0E);
    
    // Unlock CRTC registers
    outb(0x3D4, 0x11);
    outb(0x3D5, inb(0x3D5) & 0x7F);
    
    // CRTC registers
    static const uint8_t crtc_regs[] = {
        0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
        0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
        0xFF
    };
    
    for (int i = 0; i < 25; i++) {
        outb(0x3D4, i);
        outb(0x3D5, crtc_regs[i]);
    }
    
    // Graphics Controller registers
    static const uint8_t gc_regs[] = {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF
    };
    
    for (int i = 0; i < 9; i++) {
        outb(0x3CE, i);
        outb(0x3CF, gc_regs[i]);
    }
    
    // Attribute Controller registers
    static const uint8_t ac_regs[] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
        0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
        0x41, 0x00, 0x0F, 0x00, 0x00
    };
    
    inb(0x3DA);  // Reset flip-flop
    for (int i = 0; i < 21; i++) {
        outb(0x3C0, i);
        outb(0x3C0, ac_regs[i]);
    }
    outb(0x3C0, 0x20);  // Enable video
    
    graphics_enabled = true;
}

// Set VGA text mode (Mode 3)
void graphics_set_text_mode(void) {
    // This is simplified - in practice you'd need full register programming
    // For now, we'll stay in text mode which is the default
    graphics_enabled = false;
}

// Initialize graphics
void graphics_init(void) {
    // Stay in text mode for now (most reliable)
    // graphics_set_mode_13h();  // Uncomment to enable graphics
    printf("[Graphics] Text mode active (VGA available)\n");
}

// Put a pixel (only works in graphics mode)
void put_pixel(uint16_t x, uint16_t y, uint8_t color) {
    if (!graphics_enabled) return;
    if (x < GFX_WIDTH && y < GFX_HEIGHT) {
        framebuffer[y * GFX_WIDTH + x] = color;
    }
}

// Draw a filled rectangle
void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t color) {
    if (!graphics_enabled) return;
    
    for (uint16_t cy = y; cy < y + h && cy < GFX_HEIGHT; cy++) {
        for (uint16_t cx = x; cx < x + w && cx < GFX_WIDTH; cx++) {
            framebuffer[cy * GFX_WIDTH + cx] = color;
        }
    }
}

// Clear screen
void graphics_clear(uint8_t color) {
    if (!graphics_enabled) return;
    
    for (int i = 0; i < GFX_WIDTH * GFX_HEIGHT; i++) {
        framebuffer[i] = color;
    }
}

// Draw a line (Bresenham's algorithm)
void draw_line(int x0, int y0, int x1, int y1, uint8_t color) {
    if (!graphics_enabled) return;
    
    int dx = x1 - x0;
    int dy = y1 - y0;
    int sx = (dx > 0) ? 1 : -1;
    int sy = (dy > 0) ? 1 : -1;
    
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    
    int err = dx - dy;
    
    while (1) {
        put_pixel(x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

// Check if graphics mode is enabled
bool graphics_is_enabled(void) {
    return graphics_enabled;
}
