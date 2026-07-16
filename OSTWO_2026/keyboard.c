// File: keyboard.c
// PS/2 Keyboard driver

#include "types.h"
#include "events.h"

// External functions
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void irq_unmask(uint8_t irq);

// Keyboard ports
#define KB_DATA_PORT    0x60
#define KB_STATUS_PORT  0x64
#define KB_COMMAND_PORT 0x64

// Key state
static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;

// US keyboard layout scancode to ASCII (lowercase)
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

// US keyboard layout scancode to ASCII (uppercase/shifted)
static const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

// Convert scancode to ASCII
static char scancode_to_char(uint8_t scancode) {
    if (scancode >= sizeof(scancode_to_ascii)) {
        return 0;
    }
    
    bool use_upper = shift_pressed;
    
    // Handle caps lock for letters
    if (caps_lock) {
        char c = scancode_to_ascii[scancode];
        if (c >= 'a' && c <= 'z') {
            use_upper = !use_upper;
        }
    }
    
    if (use_upper) {
        return scancode_to_ascii_shift[scancode];
    }
    return scancode_to_ascii[scancode];
}

// Keyboard interrupt handler
static void keyboard_callback(registers_t* regs) {
    (void)regs;
    
    uint8_t scancode = inb(KB_DATA_PORT);
    
    // Check for key release (bit 7 set)
    bool released = (scancode & 0x80) != 0;
    scancode &= 0x7F;
    
    // Handle modifier keys
    switch (scancode) {
        case 0x2A:  // Left Shift
        case 0x36:  // Right Shift
            shift_pressed = !released;
            return;
        case 0x1D:  // Ctrl
            ctrl_pressed = !released;
            return;
        case 0x38:  // Alt
            alt_pressed = !released;
            return;
        case 0x3A:  // Caps Lock (toggle on press)
            if (!released) {
                caps_lock = !caps_lock;
            }
            return;
    }
    
    // Only process key presses, not releases
    if (released) {
        return;
    }
    
    // Convert to ASCII
    char c = scancode_to_char(scancode);
    
    if (c) {
        // Create and push event
        input_event_t event;
        event.type = EVENT_TYPE_KEY_DOWN;
        event.data[0] = c;
        event.data[1] = scancode;
        event.data[2] = shift_pressed;
        event.data[3] = ctrl_pressed;
        event.data[4] = alt_pressed;
        push_event(event);
    }
}

// Initialize keyboard
void keyboard_init(void) {
    // Register handler
    register_interrupt_handler(33, keyboard_callback);
    
    // Wait for keyboard controller
    while (inb(KB_STATUS_PORT) & 0x02);
    
    // Enable keyboard
    outb(KB_COMMAND_PORT, 0xAE);
    
    // Wait and clear buffer
    while (inb(KB_STATUS_PORT) & 0x01) {
        inb(KB_DATA_PORT);
    }
    
    // Unmask IRQ1
    irq_unmask(1);
    
    printf("[Keyboard] Initialized\n");
}

// Get modifier state
bool keyboard_shift_pressed(void) { return shift_pressed; }
bool keyboard_ctrl_pressed(void) { return ctrl_pressed; }
bool keyboard_alt_pressed(void) { return alt_pressed; }
