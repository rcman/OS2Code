// File: vbe.c
// VBE (VESA BIOS Extensions) Support

#include "vbe.h"
#include "types.h"

// Global VBE mode info
vbe_mode_info_t vbe_mode_info;

// Initialize VBE from multiboot info
void vbe_init_from_multiboot(uint32_t mboot_flags, multiboot_framebuffer_t* fb_info) {
    // Check if framebuffer info is available (bit 12 of flags)
    if (!(mboot_flags & (1 << 12))) {
        vbe_mode_info.enabled = 0;
        return;
    }

    if (!fb_info) {
        vbe_mode_info.enabled = 0;
        return;
    }

    // Extract framebuffer information
    vbe_mode_info.framebuffer = (uint32_t)fb_info->framebuffer_addr;
    vbe_mode_info.width = (uint16_t)fb_info->framebuffer_width;
    vbe_mode_info.height = (uint16_t)fb_info->framebuffer_height;
    vbe_mode_info.bpp = fb_info->framebuffer_bpp;
    vbe_mode_info.pitch = (uint16_t)fb_info->framebuffer_pitch;
    vbe_mode_info.memory_model = fb_info->framebuffer_type;

    // Extract color field information (for RGB modes)
    if (fb_info->framebuffer_type == 1) {  // RGB color
        vbe_mode_info.red_field_pos = fb_info->color_info[0];
        vbe_mode_info.red_mask_size = fb_info->color_info[1];
        vbe_mode_info.green_field_pos = fb_info->color_info[2];
        vbe_mode_info.green_mask_size = fb_info->color_info[3];
        vbe_mode_info.blue_field_pos = fb_info->color_info[4];
        vbe_mode_info.blue_mask_size = fb_info->color_info[5];
    }

    vbe_mode_info.enabled = 1;
}

// Get VBE mode info
vbe_mode_info_t* vbe_get_mode_info(void) {
    return &vbe_mode_info;
}
