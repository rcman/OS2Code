// File: vbe.h
// VBE (VESA BIOS Extensions) Support

#ifndef VBE_H
#define VBE_H

#include "types.h"

// VBE mode attributes
#define VBE_ATTR_SUPPORTED      0x01
#define VBE_ATTR_COLOR          0x08
#define VBE_ATTR_GRAPHICS       0x10
#define VBE_ATTR_NOT_VGA        0x20
#define VBE_ATTR_LINEAR_FB      0x80

// VBE mode info structure (passed from boot.asm)
typedef struct {
    uint32_t framebuffer;       // Linear framebuffer address
    uint16_t width;             // Screen width in pixels
    uint16_t height;            // Screen height in pixels
    uint8_t bpp;                // Bits per pixel (8, 16, 24, 32)
    uint8_t red_mask_size;      // Red mask size
    uint8_t red_field_pos;      // Red field position
    uint8_t green_mask_size;    // Green mask size
    uint8_t green_field_pos;    // Green field position
    uint8_t blue_mask_size;     // Blue mask size
    uint8_t blue_field_pos;     // Blue field position
    uint16_t pitch;             // Bytes per scanline
    uint8_t memory_model;       // Memory model (6 = direct color)
    uint8_t enabled;            // 1 if VBE was successfully initialized
} __attribute__((packed)) vbe_mode_info_t;

// VBE info block (used in boot.asm)
typedef struct {
    char signature[4];          // "VESA" or "VBE2"
    uint16_t version;           // VBE version (0x0300 for VBE 3.0)
    uint32_t oem_string_ptr;    // Pointer to OEM string
    uint32_t capabilities;      // Capabilities flags
    uint32_t video_modes_ptr;   // Pointer to video mode list
    uint16_t total_memory;      // Total memory in 64KB blocks
    uint16_t oem_software_rev;  // OEM software revision
    uint32_t oem_vendor_name_ptr;
    uint32_t oem_product_name_ptr;
    uint32_t oem_product_rev_ptr;
    uint8_t reserved[222];      // Reserved
    uint8_t oem_data[256];      // OEM data
} __attribute__((packed)) vbe_info_block_t;

// VBE mode info block (detailed mode information)
typedef struct {
    uint16_t attributes;        // Mode attributes
    uint8_t window_a;           // Window A attributes
    uint8_t window_b;           // Window B attributes
    uint16_t granularity;       // Window granularity
    uint16_t window_size;       // Window size
    uint16_t segment_a;         // Window A segment
    uint16_t segment_b;         // Window B segment
    uint32_t win_func_ptr;      // Window function pointer
    uint16_t pitch;             // Bytes per scanline
    uint16_t width;             // Width in pixels
    uint16_t height;            // Height in pixels
    uint8_t w_char;             // Character cell width
    uint8_t y_char;             // Character cell height
    uint8_t planes;             // Number of planes
    uint8_t bpp;                // Bits per pixel
    uint8_t banks;              // Number of banks
    uint8_t memory_model;       // Memory model type
    uint8_t bank_size;          // Bank size in KB
    uint8_t image_pages;        // Number of image pages
    uint8_t reserved0;          // Reserved
    uint8_t red_mask;           // Red mask size
    uint8_t red_position;       // Red field position
    uint8_t green_mask;         // Green mask size
    uint8_t green_position;     // Green field position
    uint8_t blue_mask;          // Blue mask size
    uint8_t blue_position;      // Blue field position
    uint8_t reserved_mask;      // Reserved mask size
    uint8_t reserved_position;  // Reserved field position
    uint8_t direct_color_attributes; // Direct color mode attributes
    uint32_t framebuffer;       // Linear framebuffer address
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
    uint8_t reserved1[206];     // Reserved
} __attribute__((packed)) vbe_mode_info_block_t;

// Global VBE mode info (set by boot.asm)
extern vbe_mode_info_t vbe_mode_info;

// Multiboot framebuffer info structure
typedef struct {
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
} __attribute__((packed)) multiboot_framebuffer_t;

// Initialize VBE from multiboot info
void vbe_init_from_multiboot(uint32_t mboot_flags, multiboot_framebuffer_t* fb_info);

// Get VBE mode info
vbe_mode_info_t* vbe_get_mode_info(void);

#endif // VBE_H
