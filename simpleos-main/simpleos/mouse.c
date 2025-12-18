// File: mouse.c
// PS/2 Mouse driver

#include "types.h"
#include "events.h"

// External functions
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void irq_unmask(uint8_t irq);
extern void io_wait(void);

// Mouse ports
#define MOUSE_DATA_PORT     0x60
#define MOUSE_STATUS_PORT   0x64
#define MOUSE_COMMAND_PORT  0x64

// Screen bounds
#define SCREEN_WIDTH  1024
#define SCREEN_HEIGHT 768

// Mouse state
static int32_t mouse_x = SCREEN_WIDTH / 2;
static int32_t mouse_y = SCREEN_HEIGHT / 2;
static uint8_t mouse_cycle = 0;
static uint8_t mouse_bytes[3];
static bool mouse_buttons[3] = {false, false, false};

// Wait for mouse controller to be ready
static void mouse_wait(uint8_t type) {
    uint32_t timeout = 100000;
    if (type == 0) {
        // Wait for output buffer to be full
        while (timeout--) {
            if (inb(MOUSE_STATUS_PORT) & 0x01) return;
        }
    } else {
        // Wait for input buffer to be empty
        while (timeout--) {
            if (!(inb(MOUSE_STATUS_PORT) & 0x02)) return;
        }
    }
}

// Write to mouse
static void mouse_write(uint8_t data) {
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0xD4);
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, data);
}

// Read from mouse
static uint8_t mouse_read(void) {
    mouse_wait(0);
    return inb(MOUSE_DATA_PORT);
}

// Mouse interrupt handler
static void mouse_callback(registers_t* regs) {
    (void)regs;
    
    uint8_t status = inb(MOUSE_STATUS_PORT);
    
    // Check if data is from mouse (bit 5)
    if (!(status & 0x20)) {
        return;
    }
    
    uint8_t data = inb(MOUSE_DATA_PORT);
    
    switch (mouse_cycle) {
        case 0:
            mouse_bytes[0] = data;
            // Verify it's a valid first byte (bit 3 should be set)
            if (data & 0x08) {
                mouse_cycle++;
            }
            break;
            
        case 1:
            mouse_bytes[1] = data;
            mouse_cycle++;
            break;
            
        case 2:
            mouse_bytes[2] = data;
            mouse_cycle = 0;
            
            // Process the complete packet
            int32_t dx = mouse_bytes[1];
            int32_t dy = mouse_bytes[2];
            
            // Handle sign extension
            if (mouse_bytes[0] & 0x10) dx |= 0xFFFFFF00;
            if (mouse_bytes[0] & 0x20) dy |= 0xFFFFFF00;
            
            // Check for overflow
            if (mouse_bytes[0] & 0x40) dx = 0;  // X overflow
            if (mouse_bytes[0] & 0x80) dy = 0;  // Y overflow
            
            // Update position (Y is inverted on PS/2 mouse)
            mouse_x += dx;
            mouse_y -= dy;
            
            // Clamp to screen bounds
            if (mouse_x < 0) mouse_x = 0;
            if (mouse_x >= SCREEN_WIDTH) mouse_x = SCREEN_WIDTH - 1;
            if (mouse_y < 0) mouse_y = 0;
            if (mouse_y >= SCREEN_HEIGHT) mouse_y = SCREEN_HEIGHT - 1;
            
            // Check buttons
            bool left = (mouse_bytes[0] & 0x01) != 0;
            bool right = (mouse_bytes[0] & 0x02) != 0;
            bool middle = (mouse_bytes[0] & 0x04) != 0;
            
            // Generate click events on button press
            if (left && !mouse_buttons[0]) {
                input_event_t event;
                event.type = EVENT_TYPE_MOUSE_CLICK;
                *(uint16_t*)&event.data[0] = (uint16_t)mouse_x;
                *(uint16_t*)&event.data[2] = (uint16_t)mouse_y;
                event.data[4] = 0;  // Left button
                push_event(event);
            }
            if (right && !mouse_buttons[1]) {
                input_event_t event;
                event.type = EVENT_TYPE_MOUSE_CLICK;
                *(uint16_t*)&event.data[0] = (uint16_t)mouse_x;
                *(uint16_t*)&event.data[2] = (uint16_t)mouse_y;
                event.data[4] = 1;  // Right button
                push_event(event);
            }
            if (middle && !mouse_buttons[2]) {
                input_event_t event;
                event.type = EVENT_TYPE_MOUSE_CLICK;
                *(uint16_t*)&event.data[0] = (uint16_t)mouse_x;
                *(uint16_t*)&event.data[2] = (uint16_t)mouse_y;
                event.data[4] = 2;  // Middle button
                push_event(event);
            }
            
            mouse_buttons[0] = left;
            mouse_buttons[1] = right;
            mouse_buttons[2] = middle;
            
            // Generate move event
            if (dx != 0 || dy != 0) {
                input_event_t event;
                event.type = EVENT_TYPE_MOUSE_MOVE;
                *(uint16_t*)&event.data[0] = (uint16_t)mouse_x;
                *(uint16_t*)&event.data[2] = (uint16_t)mouse_y;
                event.data[4] = mouse_buttons[0];
                event.data[5] = mouse_buttons[1];
                push_event(event);
            }
            break;
    }
}

// Initialize mouse
void mouse_init(void) {
    // Register handler
    register_interrupt_handler(44, mouse_callback);
    
    // Enable auxiliary mouse device
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0xA8);
    
    // Enable interrupts
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0x20);
    mouse_wait(0);
    uint8_t status = inb(MOUSE_DATA_PORT) | 0x02;
    mouse_wait(1);
    outb(MOUSE_COMMAND_PORT, 0x60);
    mouse_wait(1);
    outb(MOUSE_DATA_PORT, status);
    
    // Use default settings
    mouse_write(0xF6);
    mouse_read();
    
    // Enable mouse
    mouse_write(0xF4);
    mouse_read();
    
    // Unmask IRQ12
    irq_unmask(12);
    
    printf("[Mouse] Initialized at (%d, %d)\n", mouse_x, mouse_y);
}

// Get mouse position
void mouse_get_position(int32_t* x, int32_t* y) {
    *x = mouse_x;
    *y = mouse_y;
}

// Get button state
bool mouse_button_pressed(int button) {
    if (button >= 0 && button < 3) {
        return mouse_buttons[button];
    }
    return false;
}
