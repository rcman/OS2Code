// File: bga.c
// Bochs Graphics Adapter (BGA / Bochs VBE DISPI) driver implementation

#include "bga.h"
#include "vbe.h"
#include "pci.h"

// External functions
extern void printf(const char* format, ...);
extern void outw(uint16_t port, uint16_t value);
extern uint16_t inw(uint16_t port);

// PCI IDs of display devices that implement the DISPI interface
#define PCI_VENDOR_QEMU_BOCHS   0x1234  // QEMU stdvga / Bochs VBE
#define PCI_DEVICE_QEMU_STDVGA  0x1111
#define PCI_VENDOR_REDHAT_QXL   0x1B36  // QXL (VGA-compat mode)
#define PCI_DEVICE_QXL          0x0100

// Set by bga_init() on success; bga_activate() is a no-op until then
static int bga_ready = 0;

static void bga_write_reg(uint16_t index, uint16_t value) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    outw(VBE_DISPI_IOPORT_DATA, value);
}

static uint16_t bga_read_reg(uint16_t index) {
    outw(VBE_DISPI_IOPORT_INDEX, index);
    return inw(VBE_DISPI_IOPORT_DATA);
}

int bga_available(void) {
    uint16_t id = bga_read_reg(VBE_DISPI_INDEX_ID);
    return (id >= VBE_DISPI_ID0 && id <= VBE_DISPI_ID5);
}

// Scan the PCI bus for a DISPI-capable display device and return the
// physical address of its linear framebuffer (BAR0), or 0 if not found.
// Uses raw config-space reads so it works before pci_init().
static uint32_t bga_find_framebuffer(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint16_t vendor = pci_config_read_word((uint8_t)bus, dev, 0, 0x00);
            if (vendor == 0xFFFF) {
                continue;
            }
            uint16_t device = pci_config_read_word((uint8_t)bus, dev, 0, 0x02);

            if ((vendor == PCI_VENDOR_QEMU_BOCHS && device == PCI_DEVICE_QEMU_STDVGA) ||
                (vendor == PCI_VENDOR_REDHAT_QXL && device == PCI_DEVICE_QXL)) {
                uint32_t bar0 = pci_config_read_dword((uint8_t)bus, dev, 0, 0x10);
                if (bar0 & 0x1) {
                    continue;  // I/O BAR, not memory - keep looking
                }
                return bar0 & 0xFFFFFFF0;
            }
        }
    }
    return 0;
}

int bga_init(uint16_t width, uint16_t height, uint8_t bpp) {
    uint16_t id = bga_read_reg(VBE_DISPI_INDEX_ID);
    if (id < VBE_DISPI_ID0 || id > VBE_DISPI_ID5) {
        printf("[BGA] No Bochs display adapter detected (ID=0x%x)\n", id);
        return -1;
    }
    printf("[BGA] Bochs display adapter found (DISPI ID 0x%x)\n", id);

    uint32_t fb_phys = bga_find_framebuffer();
    if (fb_phys == 0) {
        printf("[BGA] ERROR: Could not locate framebuffer BAR via PCI\n");
        return -1;
    }

    // Mode changes require the adapter to be disabled
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    bga_write_reg(VBE_DISPI_INDEX_XRES, width);
    bga_write_reg(VBE_DISPI_INDEX_YRES, height);
    bga_write_reg(VBE_DISPI_INDEX_BPP, bpp);
    bga_write_reg(VBE_DISPI_INDEX_VIRT_WIDTH, width);
    bga_write_reg(VBE_DISPI_INDEX_VIRT_HEIGHT, height);
    bga_write_reg(VBE_DISPI_INDEX_X_OFFSET, 0);
    bga_write_reg(VBE_DISPI_INDEX_Y_OFFSET, 0);
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    // Read back what the adapter actually accepted
    uint16_t got_w = bga_read_reg(VBE_DISPI_INDEX_XRES);
    uint16_t got_h = bga_read_reg(VBE_DISPI_INDEX_YRES);
    uint16_t got_bpp = bga_read_reg(VBE_DISPI_INDEX_BPP);
    uint16_t virt_w = bga_read_reg(VBE_DISPI_INDEX_VIRT_WIDTH);
    if (virt_w < got_w) {
        virt_w = got_w;
    }

    // Return to text mode for the rest of the boot messages; the mode
    // registers keep their values and bga_activate() re-enables the LFB
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);

    // Publish the mode through the shared VBE info block so the rest of
    // the kernel (framebuffer mapping, vga_gfx VBE path) works unchanged
    vbe_mode_info.framebuffer = fb_phys;
    vbe_mode_info.width = got_w;
    vbe_mode_info.height = got_h;
    vbe_mode_info.bpp = (uint8_t)got_bpp;
    vbe_mode_info.pitch = (uint16_t)(virt_w * (got_bpp / 8));
    vbe_mode_info.memory_model = 1;  // Direct RGB
    vbe_mode_info.red_field_pos = 16;
    vbe_mode_info.red_mask_size = 8;
    vbe_mode_info.green_field_pos = 8;
    vbe_mode_info.green_mask_size = 8;
    vbe_mode_info.blue_field_pos = 0;
    vbe_mode_info.blue_mask_size = 8;
    vbe_mode_info.enabled = 1;

    bga_ready = 1;
    printf("[BGA] Mode programmed: %dx%dx%d, LFB at 0x%x, pitch %d\n",
           got_w, got_h, got_bpp, fb_phys, vbe_mode_info.pitch);
    return 0;
}

void bga_activate(void) {
    if (!bga_ready) {
        return;
    }
    bga_write_reg(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);
}
