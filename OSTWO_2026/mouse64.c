// File: mouse64.c
// PS/2 mouse driver for the 64-bit kernel (Phase B.7).
//
// IRQ12 assembles 3-byte packets and updates the shared cursor
// position and button state; the GUI loop polls mouse_moved.

#include <stdint.h>

static inline void outb(uint16_t p, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(p));
}
static inline uint8_t inb(uint16_t p) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(p));
    return v;
}

// Shared state read by the GUI
volatile int mouse_x = 512, mouse_y = 384;
volatile int mouse_buttons;
volatile int mouse_moved;

static uint8_t packet[3];
static int phase;

// Bounded waits so a missing device can't hang the kernel.
static void wait_write(void) { for (int i = 0; i < 100000 && (inb(0x64) & 2); i++) { } }
static void wait_read(void)  { for (int i = 0; i < 100000 && !(inb(0x64) & 1); i++) { } }

static void mouse_cmd(uint8_t cmd) {
    wait_write(); outb(0x64, 0xD4);      // address the mouse
    wait_write(); outb(0x60, cmd);
    wait_read();  inb(0x60);             // consume ACK (0xFA)
}

void mouse64_init(void) {
    // Interrupts OFF during setup: otherwise the IRQ12 handler steals
    // the ACK bytes that these polling reads are waiting for, hanging
    // the kernel. Re-enable at the end.
    __asm__ volatile("cli");

    wait_write(); outb(0x64, 0xA8);      // enable auxiliary device

    wait_write(); outb(0x64, 0x20);      // read controller config
    wait_read();  uint8_t cfg = inb(0x60);
    cfg |= 2;                            // enable IRQ12
    cfg &= ~0x20;                        // enable mouse clock
    wait_write(); outb(0x64, 0x60);
    wait_write(); outb(0x60, cfg);

    mouse_cmd(0xF6);                     // set defaults
    mouse_cmd(0xF4);                     // enable data reporting
    phase = 0;

    inb(0x60);                           // drain any stray byte
    __asm__ volatile("sti");
}

// Called from the IRQ12 stub (irq64_mouse); EOI sent by the stub.
void mouse64_handler(void) {
    uint8_t b = inb(0x60);
    switch (phase) {
        case 0:
            if (!(b & 0x08)) return;     // resync: bit3 always 1 in byte0
            packet[0] = b;
            phase = 1;
            break;
        case 1:
            packet[1] = b;
            phase = 2;
            break;
        case 2: {
            packet[2] = b;
            phase = 0;
            int dx = packet[1];
            int dy = packet[2];
            if (packet[0] & 0x10) dx |= ~0xFF;   // sign-extend X
            if (packet[0] & 0x20) dy |= ~0xFF;   // sign-extend Y
            int nx = mouse_x + dx;
            int ny = mouse_y - dy;               // screen Y is inverted
            if (nx < 0) nx = 0; if (nx > 1023) nx = 1023;
            if (ny < 0) ny = 0; if (ny > 767) ny = 767;
            mouse_x = nx;
            mouse_y = ny;
            mouse_buttons = packet[0] & 7;
            mouse_moved = 1;
            break;
        }
    }
}
