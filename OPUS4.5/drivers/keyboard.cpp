#include "../drivers/keyboard.h"

Keyboard keyboard;

// I/O port functions
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

// US keyboard layout - scancode to ASCII
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0
};

static const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0
};

void Keyboard::init() {
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;
    
    // Wait for keyboard controller to be ready
    while (inb(0x64) & 0x02);
}

bool Keyboard::hasKey() {
    return (inb(0x64) & 0x01) != 0;
}

char Keyboard::scanToChar(uint8_t scancode) {
    if (scancode >= sizeof(scancode_to_ascii)) {
        return 0;
    }
    
    bool use_shift = shift_pressed;
    
    // Handle caps lock for letters
    if (caps_lock) {
        char c = scancode_to_ascii[scancode];
        if (c >= 'a' && c <= 'z') {
            use_shift = !use_shift;
        }
    }
    
    if (use_shift) {
        return scancode_to_ascii_shift[scancode];
    }
    return scancode_to_ascii[scancode];
}

char Keyboard::getchar() {
    while (true) {
        // Wait for a key (busy wait since we have no interrupts)
        while (!hasKey()) {
            // Busy wait - no hlt since we have no interrupts
        }
        
        uint8_t scancode = inb(0x60);
        
        // Key release (high bit set)
        if (scancode & 0x80) {
            scancode &= 0x7F;
            // Handle modifier key releases
            if (scancode == 0x2A || scancode == 0x36) {  // Left/Right Shift
                shift_pressed = false;
            } else if (scancode == 0x1D) {  // Ctrl
                ctrl_pressed = false;
            } else if (scancode == 0x38) {  // Alt
                alt_pressed = false;
            }
            continue;
        }
        
        // Handle modifier key presses
        if (scancode == 0x2A || scancode == 0x36) {  // Left/Right Shift
            shift_pressed = true;
            continue;
        } else if (scancode == 0x1D) {  // Ctrl
            ctrl_pressed = true;
            continue;
        } else if (scancode == 0x38) {  // Alt
            alt_pressed = true;
            continue;
        } else if (scancode == 0x3A) {  // Caps Lock
            caps_lock = !caps_lock;
            continue;
        }
        
        char c = scanToChar(scancode);
        if (c != 0) {
            return c;
        }
    }
}
