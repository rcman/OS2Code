// File: vga_gfx.c
// VGA Graphics Mode Driver Implementation

#include "vga_gfx.h"
#include "vbe.h"
#include "bga.h"

// External I/O functions
extern void outb(uint16_t port, uint8_t value);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);
extern void* pmm_alloc_page(void);
extern void pmm_free_page(uint32_t addr);

// Current mode information. Initialized with mode-13h-like defaults
// so consumers (mouse bounds, GUI dimensions) never see width/height
// of 0 before the first vga_set_mode() call.
static vga_mode_info_t current_mode = {
    .width = 320,
    .height = 200,
    .bpp = 8,
    .mode = 0x13,
    .framebuffer = (uint8_t*)0xA0000,
    .pitch = 320,
};
static int vbe_mode_active = 0;

// Double buffering
static uint8_t* back_buffer = NULL;
static int double_buffer_enabled = 0;

// Simple absolute value
static int abs(int x) {
    return x < 0 ? -x : x;
}

// Simple min/max
static int min(int a, int b) {
    return a < b ? a : b;
}

static int max(int a, int b) {
    return a > b ? a : b;
}

// Initialize VGA graphics subsystem
void vga_gfx_init(void) {
    // Start in text mode
    current_mode.width = 80;
    current_mode.height = 25;
    current_mode.bpp = 4;  // 16 colors
    current_mode.mode = VGA_MODE_TEXT_80x25;
    current_mode.framebuffer = (uint8_t*)VGA_TEXT_MEMORY;
    current_mode.pitch = 160;  // 80 chars * 2 bytes per char

    printf("[VGA-GFX] Graphics subsystem initialized\n");
}

// Set VGA video mode
int vga_set_mode(uint8_t mode) {
    // Check if VBE framebuffer is available from bootloader
    vbe_mode_info_t* vbe_info = vbe_get_mode_info();

    if (vbe_info && vbe_info->enabled && mode == VGA_MODE_320x200x256) {
        // Use VBE framebuffer instead of Mode 13h.
        // If the framebuffer came from the BGA driver (QEMU -kernel boot),
        // the display is still in text mode; switch it on now.
        bga_activate();
        current_mode.width = vbe_info->width;
        current_mode.height = vbe_info->height;
        current_mode.bpp = vbe_info->bpp;
        current_mode.mode = mode;
        current_mode.framebuffer = (uint8_t*)vbe_info->framebuffer;
        current_mode.pitch = vbe_info->pitch;
        vbe_mode_active = 1;

        printf("[VGA-GFX] Using VBE mode: %dx%dx%d, FB=0x%x\n",
               vbe_info->width, vbe_info->height, vbe_info->bpp, vbe_info->framebuffer);
        return 0;
    }

    // Fall back to legacy Mode 13h
    vbe_mode_active = 0;

    if (mode == VGA_MODE_320x200x256) {
        // Mode 13h: 320x200 256-color graphics mode

        // Write mode to VGA registers
        outb(VGA_MISC_WRITE, 0x63);

        // Sequencer registers
        outb(VGA_SEQ_INDEX, 0x00); outb(VGA_SEQ_DATA, 0x03);
        outb(VGA_SEQ_INDEX, 0x01); outb(VGA_SEQ_DATA, 0x01);
        outb(VGA_SEQ_INDEX, 0x02); outb(VGA_SEQ_DATA, 0x0F);
        outb(VGA_SEQ_INDEX, 0x03); outb(VGA_SEQ_DATA, 0x00);
        outb(VGA_SEQ_INDEX, 0x04); outb(VGA_SEQ_DATA, 0x0E);

        // Unlock CRTC registers
        outb(VGA_CRTC_INDEX, 0x03); outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
        outb(VGA_CRTC_INDEX, 0x11); outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);

        // CRTC registers for 320x200
        uint8_t crtc_regs[] = {
            0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
            0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF
        };
        for (int i = 0; i < 25; i++) {
            outb(VGA_CRTC_INDEX, i);
            outb(VGA_CRTC_DATA, crtc_regs[i]);
        }

        // Graphics controller registers
        outb(VGA_GC_INDEX, 0x00); outb(VGA_GC_DATA, 0x00);
        outb(VGA_GC_INDEX, 0x01); outb(VGA_GC_DATA, 0x00);
        outb(VGA_GC_INDEX, 0x02); outb(VGA_GC_DATA, 0x00);
        outb(VGA_GC_INDEX, 0x03); outb(VGA_GC_DATA, 0x00);
        outb(VGA_GC_INDEX, 0x04); outb(VGA_GC_DATA, 0x00);
        outb(VGA_GC_INDEX, 0x05); outb(VGA_GC_DATA, 0x40);
        outb(VGA_GC_INDEX, 0x06); outb(VGA_GC_DATA, 0x05);
        outb(VGA_GC_INDEX, 0x07); outb(VGA_GC_DATA, 0x0F);
        outb(VGA_GC_INDEX, 0x08); outb(VGA_GC_DATA, 0xFF);

        // Attribute controller registers
        for (int i = 0; i < 16; i++) {
            inb(VGA_INPUT_STATUS);  // Reset flip-flop
            outb(VGA_ATTR_INDEX, i);
            outb(VGA_ATTR_DATA_WRITE, i);
        }
        inb(VGA_INPUT_STATUS);
        outb(VGA_ATTR_INDEX, 0x10); outb(VGA_ATTR_DATA_WRITE, 0x41);
        inb(VGA_INPUT_STATUS);
        outb(VGA_ATTR_INDEX, 0x11); outb(VGA_ATTR_DATA_WRITE, 0x00);
        inb(VGA_INPUT_STATUS);
        outb(VGA_ATTR_INDEX, 0x12); outb(VGA_ATTR_DATA_WRITE, 0x0F);
        inb(VGA_INPUT_STATUS);
        outb(VGA_ATTR_INDEX, 0x13); outb(VGA_ATTR_DATA_WRITE, 0x00);
        inb(VGA_INPUT_STATUS);
        outb(VGA_ATTR_INDEX, 0x14); outb(VGA_ATTR_DATA_WRITE, 0x00);
        inb(VGA_INPUT_STATUS);
        outb(VGA_ATTR_INDEX, 0x20);  // Enable video

        // Update mode info
        current_mode.width = 320;
        current_mode.height = 200;
        current_mode.bpp = 8;  // 256 colors
        current_mode.mode = mode;
        current_mode.framebuffer = (uint8_t*)VGA_GFX_MEMORY;
        current_mode.pitch = 320;

        // Set default palette
        vga_set_default_palette();

        printf("[VGA-GFX] Set mode 13h (320x200x256)\n");
        return 0;
    }

    return -1;  // Unsupported mode
}

// Get current mode information
vga_mode_info_t* vga_get_mode_info(void) {
    return &current_mode;
}

// Convert 8-bit palette color to 32-bit RGB
static uint32_t palette_to_rgb(uint8_t color) {
    // Standard VGA 16-color palette
    static const uint32_t vga_palette[16] = {
        0x000000,  // 0: Black
        0x0000AA,  // 1: Blue
        0x00AA00,  // 2: Green
        0x00AAAA,  // 3: Cyan
        0xAA0000,  // 4: Red
        0xAA00AA,  // 5: Magenta
        0xAA5500,  // 6: Brown
        0xAAAAAA,  // 7: Light Gray
        0x555555,  // 8: Dark Gray
        0x5555FF,  // 9: Light Blue
        0x55FF55,  // 10: Light Green
        0x55FFFF,  // 11: Light Cyan
        0xFF5555,  // 12: Light Red
        0xFF55FF,  // 13: Light Magenta
        0xFFFF55,  // 14: Yellow
        0xFFFFFF   // 15: White
    };

    // Use lookup table for standard 16 colors
    if (color < 16) {
        return vga_palette[color];
    }

    // For colors 16-255, use a simple RGB mapping
    // Colors 16-231: 6x6x6 color cube
    if (color >= 16 && color <= 231) {
        uint8_t idx = color - 16;
        uint8_t r = (idx / 36) * 51;
        uint8_t g = ((idx / 6) % 6) * 51;
        uint8_t b = (idx % 6) * 51;
        return (r << 16) | (g << 8) | b;
    }

    // Colors 232-255: grayscale
    uint8_t gray = (color - 232) * 10 + 8;
    return (gray << 16) | (gray << 8) | gray;
}

// Clear screen with color
void vga_clear_screen(uint8_t color) {
    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;
    uint32_t pixels = current_mode.width * current_mode.height;

    if (current_mode.bpp == 32) {
        // 32-bit color mode
        uint32_t* fb32 = (uint32_t*)fb;
        uint32_t rgb_color = palette_to_rgb(color);
        for (uint32_t i = 0; i < pixels; i++) {
            fb32[i] = rgb_color;
        }
    } else {
        // 8-bit color mode
        for (uint32_t i = 0; i < pixels; i++) {
            fb[i] = color;
        }
    }
}

// Plot a pixel
void vga_plot_pixel(int x, int y, uint8_t color) {
    if (x < 0 || x >= (int)current_mode.width || y < 0 || y >= (int)current_mode.height) {
        return;  // Out of bounds
    }

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;

    if (current_mode.bpp == 32) {
        // 32-bit color mode
        uint32_t* fb32 = (uint32_t*)fb;
        uint32_t offset = y * (current_mode.pitch / 4) + x;
        fb32[offset] = palette_to_rgb(color);
    } else {
        // 8-bit color mode
        fb[y * current_mode.pitch + x] = color;
    }
}

// Get pixel color (8-bit palette index or approximation in 32-bit mode)
uint8_t vga_get_pixel(int x, int y) {
    if (x < 0 || x >= (int)current_mode.width || y < 0 || y >= (int)current_mode.height) {
        return 0;  // Out of bounds
    }

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;

    if (current_mode.bpp == 32) {
        // 32-bit color mode - read actual pixel value
        uint32_t* fb32 = (uint32_t*)fb;
        uint32_t offset = y * (current_mode.pitch / 4) + x;
        uint32_t rgb = fb32[offset];

        // Convert RGB back to palette index (approximation)
        // For cursor save/restore, we'll return a best-match palette color
        // In 32-bit mode, we should ideally save the full RGB value
        // For now, return the blue component as a grayscale approximation
        return (uint8_t)(rgb & 0xFF);
    } else {
        // 8-bit color mode
        return fb[y * current_mode.pitch + x];
    }
}

// Get pixel color as 32-bit value (full RGB in 32-bit mode, palette in 8-bit mode)
uint32_t vga_get_pixel32(int x, int y) {
    if (x < 0 || x >= (int)current_mode.width || y < 0 || y >= (int)current_mode.height) {
        return 0;  // Out of bounds
    }

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;

    if (current_mode.bpp == 32) {
        // 32-bit color mode - return full RGB value
        uint32_t* fb32 = (uint32_t*)fb;
        uint32_t offset = y * (current_mode.pitch / 4) + x;
        return fb32[offset];
    } else {
        // 8-bit color mode - return palette index as 32-bit
        return (uint32_t)fb[y * current_mode.pitch + x];
    }
}

// True-color filled rectangle (32-bit mode). In 8-bit mode it maps the
// RGB to the nearest low palette index so callers stay mode-agnostic.
void vga_fill_rect32(int x, int y, int width, int height, uint32_t rgb) {
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + width;  if (x1 > (int)current_mode.width)  x1 = current_mode.width;
    int y1 = y + height; if (y1 > (int)current_mode.height) y1 = current_mode.height;
    if (x0 >= x1 || y0 >= y1) return;

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;

    if (current_mode.bpp == 32) {
        for (int j = y0; j < y1; j++) {
            uint32_t* row = (uint32_t*)(fb + j * current_mode.pitch) + x0;
            for (int i = x1 - x0; i > 0; i--) *row++ = rgb;
        }
    } else {
        // Coarse RGB -> 16-color index approximation
        uint8_t r = (rgb >> 16) & 0xFF, g = (rgb >> 8) & 0xFF, b = rgb & 0xFF;
        uint8_t idx = ((r > 0x80) << 2) | ((g > 0x80) << 1) | (b > 0x80);
        if (r > 0xC0 || g > 0xC0 || b > 0xC0) idx |= 8;
        for (int j = y0; j < y1; j++) {
            uint8_t* row = fb + j * current_mode.pitch + x0;
            for (int i = x1 - x0; i > 0; i--) *row++ = idx;
        }
    }
}

// A raised (or sunken) 3D bevel - the OS/2 Workplace Shell frame look.
void vga_bevel32(int x, int y, int w, int h, int raised) {
    uint32_t tl = raised ? 0xF0F0F0 : 0x707070;
    uint32_t br = raised ? 0x707070 : 0xF0F0F0;
    vga_fill_rect32(x, y, w, 1, tl);
    vga_fill_rect32(x, y, 1, h, tl);
    vga_fill_rect32(x, y + h - 1, w, 1, br);
    vga_fill_rect32(x + w - 1, y, 1, h, br);
}

// Plot pixel from 32-bit value
void vga_plot_pixel32(int x, int y, uint32_t color) {
    if (x < 0 || x >= (int)current_mode.width || y < 0 || y >= (int)current_mode.height) {
        return;  // Out of bounds
    }

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;

    if (current_mode.bpp == 32) {
        // 32-bit color mode - write full RGB value
        uint32_t* fb32 = (uint32_t*)fb;
        uint32_t offset = y * (current_mode.pitch / 4) + x;
        fb32[offset] = color;
    } else {
        // 8-bit color mode - write palette index
        fb[y * current_mode.pitch + x] = (uint8_t)(color & 0xFF);
    }
}

// Draw a line (Bresenham's algorithm)
void vga_draw_line(int x1, int y1, int x2, int y2, uint8_t color) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1;
    int sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;

    while (1) {
        vga_plot_pixel(x1, y1, color);

        if (x1 == x2 && y1 == y2) break;

        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y1 += sy;
        }
    }
}

// Draw a rectangle (outline)
void vga_draw_rect(int x, int y, int width, int height, uint8_t color) {
    // Top and bottom edges
    for (int i = 0; i < width; i++) {
        vga_plot_pixel(x + i, y, color);
        vga_plot_pixel(x + i, y + height - 1, color);
    }

    // Left and right edges
    for (int i = 0; i < height; i++) {
        vga_plot_pixel(x, y + i, color);
        vga_plot_pixel(x + width - 1, y + i, color);
    }
}

// Draw a filled rectangle
void vga_fill_rect(int x, int y, int width, int height, uint8_t color) {
    // Clip to screen bounds once, then fill row by row without the
    // per-pixel bounds check (matters at high resolutions)
    int x0 = max(x, 0);
    int y0 = max(y, 0);
    int x1 = min(x + width, (int)current_mode.width);
    int y1 = min(y + height, (int)current_mode.height);
    if (x0 >= x1 || y0 >= y1) return;

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;

    if (current_mode.bpp == 32) {
        uint32_t rgb = palette_to_rgb(color);
        for (int j = y0; j < y1; j++) {
            uint32_t* row = (uint32_t*)(fb + j * current_mode.pitch) + x0;
            for (int i = x1 - x0; i > 0; i--) {
                *row++ = rgb;
            }
        }
    } else {
        for (int j = y0; j < y1; j++) {
            uint8_t* row = fb + j * current_mode.pitch + x0;
            for (int i = x1 - x0; i > 0; i--) {
                *row++ = color;
            }
        }
    }
}

// Draw a vertical gradient rectangle
void vga_fill_gradient(int x, int y, int width, int height,
                       uint32_t color_top, uint32_t color_bottom) {
    int x0 = max(x, 0);
    int y0 = max(y, 0);
    int x1 = min(x + width, (int)current_mode.width);
    int y1 = min(y + height, (int)current_mode.height);
    if (x0 >= x1 || y0 >= y1) return;

    if (current_mode.bpp != 32) {
        // 8-bit mode has no room for smooth gradients; approximate with
        // a solid fill of the top color's dominant palette entry
        vga_fill_rect(x, y, width, height, 3);
        return;
    }

    uint8_t* fb = double_buffer_enabled ? back_buffer : current_mode.framebuffer;
    int span = height > 1 ? height - 1 : 1;

    int r0 = (color_top >> 16) & 0xFF, g0 = (color_top >> 8) & 0xFF, b0 = color_top & 0xFF;
    int r1 = (color_bottom >> 16) & 0xFF, g1 = (color_bottom >> 8) & 0xFF, b1 = color_bottom & 0xFF;

    for (int j = y0; j < y1; j++) {
        int t = j - y;  // Position within the (unclipped) gradient
        uint32_t r = r0 + ((r1 - r0) * t) / span;
        uint32_t g = g0 + ((g1 - g0) * t) / span;
        uint32_t b = b0 + ((b1 - b0) * t) / span;
        uint32_t rgb = (r << 16) | (g << 8) | b;

        uint32_t* row = (uint32_t*)(fb + j * current_mode.pitch) + x0;
        for (int i = x1 - x0; i > 0; i--) {
            *row++ = rgb;
        }
    }
}

// Draw a circle (Midpoint circle algorithm)
void vga_draw_circle(int cx, int cy, int radius, uint8_t color) {
    int x = radius;
    int y = 0;
    int err = 0;

    while (x >= y) {
        vga_plot_pixel(cx + x, cy + y, color);
        vga_plot_pixel(cx + y, cy + x, color);
        vga_plot_pixel(cx - y, cy + x, color);
        vga_plot_pixel(cx - x, cy + y, color);
        vga_plot_pixel(cx - x, cy - y, color);
        vga_plot_pixel(cx - y, cy - x, color);
        vga_plot_pixel(cx + y, cy - x, color);
        vga_plot_pixel(cx + x, cy - y, color);

        if (err <= 0) {
            y += 1;
            err += 2 * y + 1;
        }

        if (err > 0) {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

// Draw a filled circle
void vga_fill_circle(int cx, int cy, int radius, uint8_t color) {
    for (int y = -radius; y <= radius; y++) {
        for (int x = -radius; x <= radius; x++) {
            if (x * x + y * y <= radius * radius) {
                vga_plot_pixel(cx + x, cy + y, color);
            }
        }
    }
}

#include "font8x8.h"

// Draw text character
void vga_draw_char(int x, int y, char c, uint8_t color) {
    if (c < 0 || c >= 128) return;

    for (int row = 0; row < 8; row++) {
        uint8_t byte = font_8x8[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (byte & (1 << col)) {
                vga_plot_pixel(x + col, y + row, color);
            }
        }
    }
}

// Draw text string
void vga_draw_string(int x, int y, const char* str, uint8_t color) {
    int cx = x;
    while (*str) {
        vga_draw_char(cx, y, *str, color);
        cx += 8;
        str++;
    }
}

// True-color character/string (8x8 font), for OS/2 Workplace Shell styling.
void vga_draw_char32(int x, int y, char c, uint32_t rgb) {
    if (c < 0 || c >= 128) return;
    for (int row = 0; row < 8; row++) {
        uint8_t byte = font_8x8[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (byte & (1 << col)) vga_plot_pixel32(x + col, y + row, rgb);
        }
    }
}

void vga_draw_string32(int x, int y, const char* str, uint32_t rgb) {
    int cx = x;
    while (*str) { vga_draw_char32(cx, y, *str, rgb); cx += 8; str++; }
}

// Double buffering
void vga_enable_double_buffer(void) {
    if (!back_buffer) {
        // Allocate back buffer (64KB for 320x200)
        back_buffer = (uint8_t*)pmm_alloc_page();
        if (back_buffer) {
            // Allocate more pages if needed
            for (int i = 1; i < 16; i++) {  // 16 pages = 64KB
                pmm_alloc_page();
            }
        }
    }

    if (back_buffer) {
        double_buffer_enabled = 1;
        printf("[VGA-GFX] Double buffering enabled\n");
    }
}

void vga_disable_double_buffer(void) {
    double_buffer_enabled = 0;
}

void vga_swap_buffers(void) {
    if (!double_buffer_enabled || !back_buffer) return;

    // Copy back buffer to front buffer
    uint8_t* front = current_mode.framebuffer;
    uint32_t size = current_mode.width * current_mode.height;

    for (uint32_t i = 0; i < size; i++) {
        front[i] = back_buffer[i];
    }
}

// Palette operations
void vga_set_palette(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    outb(VGA_DAC_WRITE_INDEX, index);
    outb(VGA_DAC_DATA, r >> 2);  // VGA uses 6-bit color
    outb(VGA_DAC_DATA, g >> 2);
    outb(VGA_DAC_DATA, b >> 2);
}

void vga_get_palette(uint8_t index, uint8_t* r, uint8_t* g, uint8_t* b) {
    outb(VGA_DAC_READ_INDEX, index);
    *r = inb(VGA_DAC_DATA) << 2;
    *g = inb(VGA_DAC_DATA) << 2;
    *b = inb(VGA_DAC_DATA) << 2;
}

void vga_set_default_palette(void) {
    // Set default 256-color palette
    // First 16 colors: standard VGA colors
    vga_set_palette(0, 0, 0, 0);           // Black
    vga_set_palette(1, 0, 0, 170);         // Blue
    vga_set_palette(2, 0, 170, 0);         // Green
    vga_set_palette(3, 0, 170, 170);       // Cyan
    vga_set_palette(4, 170, 0, 0);         // Red
    vga_set_palette(5, 170, 0, 170);       // Magenta
    vga_set_palette(6, 170, 85, 0);        // Brown
    vga_set_palette(7, 170, 170, 170);     // Light Gray
    vga_set_palette(8, 85, 85, 85);        // Dark Gray
    vga_set_palette(9, 85, 85, 255);       // Light Blue
    vga_set_palette(10, 85, 255, 85);      // Light Green
    vga_set_palette(11, 85, 255, 255);     // Light Cyan
    vga_set_palette(12, 255, 85, 85);      // Light Red
    vga_set_palette(13, 255, 85, 255);     // Light Magenta
    vga_set_palette(14, 255, 255, 85);     // Yellow
    vga_set_palette(15, 255, 255, 255);    // White

    // Grayscale ramp (16-31)
    for (int i = 0; i < 16; i++) {
        uint8_t val = i * 17;
        vga_set_palette(16 + i, val, val, val);
    }

    // Color gradient (32-255)
    for (int i = 32; i < 256; i++) {
        uint8_t r = ((i - 32) % 8) * 36;
        uint8_t g = (((i - 32) / 8) % 8) * 36;
        uint8_t b = (((i - 32) / 64) % 4) * 85;
        vga_set_palette(i, r, g, b);
    }
}

// Draw bitmap
void vga_draw_bitmap(int x, int y, int width, int height, const uint8_t* bitmap) {
    for (int j = 0; j < height; j++) {
        for (int i = 0; i < width; i++) {
            uint8_t color = bitmap[j * width + i];
            vga_plot_pixel(x + i, y + j, color);
        }
    }
}
