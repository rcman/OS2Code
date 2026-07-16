// File: vga_gfx.h
// VGA Graphics Mode Driver

#ifndef VGA_GFX_H
#define VGA_GFX_H

#include "types.h"

// VGA video modes
#define VGA_MODE_TEXT_80x25     0x03    // 80x25 text mode (16 colors)
#define VGA_MODE_TEXT_40x25     0x01    // 40x25 text mode (16 colors)
#define VGA_MODE_320x200x256    0x13    // 320x200 graphics (256 colors) - Mode 13h
#define VGA_MODE_640x480x16     0x12    // 640x480 graphics (16 colors)

// VGA memory addresses
#define VGA_TEXT_MEMORY     0xB8000     // Text mode video memory
#define VGA_GFX_MEMORY      0xA0000     // Graphics mode video memory

// VGA register ports
#define VGA_MISC_WRITE      0x3C2       // Miscellaneous output register
#define VGA_MISC_READ       0x3CC       // Miscellaneous output register (read)
#define VGA_SEQ_INDEX       0x3C4       // Sequencer index
#define VGA_SEQ_DATA        0x3C5       // Sequencer data
#define VGA_GC_INDEX        0x3CE       // Graphics controller index
#define VGA_GC_DATA         0x3CF       // Graphics controller data
#define VGA_CRTC_INDEX      0x3D4       // CRT controller index
#define VGA_CRTC_DATA       0x3D5       // CRT controller data
#define VGA_ATTR_INDEX      0x3C0       // Attribute controller index
#define VGA_ATTR_DATA_WRITE 0x3C0       // Attribute controller data (write)
#define VGA_ATTR_DATA_READ  0x3C1       // Attribute controller data (read)
#define VGA_INPUT_STATUS    0x3DA       // Input status register
#define VGA_DAC_WRITE_INDEX 0x3C8       // DAC write index
#define VGA_DAC_READ_INDEX  0x3C7       // DAC read index
#define VGA_DAC_DATA        0x3C9       // DAC data

// Standard VGA colors (for 16-color modes)
#define VGA_COLOR_BLACK         0
#define VGA_COLOR_BLUE          1
#define VGA_COLOR_GREEN         2
#define VGA_COLOR_CYAN          3
#define VGA_COLOR_RED           4
#define VGA_COLOR_MAGENTA       5
#define VGA_COLOR_BROWN         6
#define VGA_COLOR_LIGHT_GRAY    7
#define VGA_COLOR_DARK_GRAY     8
#define VGA_COLOR_LIGHT_BLUE    9
#define VGA_COLOR_LIGHT_GREEN   10
#define VGA_COLOR_LIGHT_CYAN    11
#define VGA_COLOR_LIGHT_RED     12
#define VGA_COLOR_LIGHT_MAGENTA 13
#define VGA_COLOR_YELLOW        14
#define VGA_COLOR_WHITE         15

// Graphics mode information
typedef struct {
    uint16_t width;         // Screen width
    uint16_t height;        // Screen height
    uint8_t  bpp;           // Bits per pixel
    uint8_t  mode;          // VGA mode number
    uint8_t* framebuffer;   // Framebuffer address
    uint32_t pitch;         // Bytes per scanline
} vga_mode_info_t;

// Initialize VGA graphics subsystem
void vga_gfx_init(void);

// Set VGA video mode
// mode: VGA_MODE_* constant
// Returns: 0 on success, -1 on error
int vga_set_mode(uint8_t mode);

// Get current mode information
vga_mode_info_t* vga_get_mode_info(void);

// Clear screen with color
void vga_clear_screen(uint8_t color);

// Plot a pixel
void vga_plot_pixel(int x, int y, uint8_t color);

// Get pixel color (8-bit palette index or approximation in 32-bit mode)
uint8_t vga_get_pixel(int x, int y);

// Get pixel color as 32-bit value (full RGB in 32-bit mode, palette in 8-bit mode)
uint32_t vga_get_pixel32(int x, int y);

// Plot pixel from 32-bit value
void vga_plot_pixel32(int x, int y, uint32_t color);

// Draw a line (Bresenham's algorithm)
void vga_draw_line(int x1, int y1, int x2, int y2, uint8_t color);

// Draw a rectangle (outline)
void vga_draw_rect(int x, int y, int width, int height, uint8_t color);

// Draw a filled rectangle
void vga_fill_rect(int x, int y, int width, int height, uint8_t color);

// Draw a vertical gradient rectangle (true color in 32-bit mode;
// falls back to a solid fill of color_top's nearest palette entry in 8-bit mode)
void vga_fill_gradient(int x, int y, int width, int height,
                       uint32_t color_top, uint32_t color_bottom);

// Draw a circle (Midpoint circle algorithm)
void vga_draw_circle(int cx, int cy, int radius, uint8_t color);

// Draw a filled circle
void vga_fill_circle(int cx, int cy, int radius, uint8_t color);

// Draw text in graphics mode (8x8 font)
void vga_draw_char(int x, int y, char c, uint8_t color);
void vga_draw_string(int x, int y, const char* str, uint8_t color);

// Double buffering
void vga_enable_double_buffer(void);
void vga_disable_double_buffer(void);
void vga_swap_buffers(void);

// Palette operations (for 256-color mode)
void vga_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b);
void vga_get_palette(uint8_t index, uint8_t* r, uint8_t* g, uint8_t* b);
void vga_set_default_palette(void);

// Bitmap operations
void vga_draw_bitmap(int x, int y, int width, int height, const uint8_t* bitmap);

#endif // VGA_GFX_H
