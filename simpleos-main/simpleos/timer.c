// File: timer.c
// Programmable Interval Timer (PIT) driver

#include "types.h"

// External functions
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);
extern void printf(const char* format, ...);
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void irq_unmask(uint8_t irq);

// PIT constants
#define PIT_CHANNEL0    0x40
#define PIT_CHANNEL1    0x41
#define PIT_CHANNEL2    0x42
#define PIT_COMMAND     0x43
#define PIT_FREQUENCY   1193182

// Timer state
static volatile uint32_t tick_count = 0;
static uint32_t timer_frequency = 0;

// External scheduler tick function
extern void scheduler_tick(void);

// Timer interrupt handler
static void timer_callback(registers_t* regs) {
    (void)regs;  // Unused
    tick_count++;

    // Call scheduler every tick
    scheduler_tick();
}

// Get current tick count
uint32_t timer_get_ticks(void) {
    return tick_count;
}

// Sleep for specified milliseconds (busy wait)
void timer_sleep(uint32_t ms) {
    uint32_t start = tick_count;
    uint32_t ticks_to_wait = (ms * timer_frequency) / 1000;
    if (ticks_to_wait == 0) ticks_to_wait = 1;
    
    while ((tick_count - start) < ticks_to_wait) {
        __asm__ volatile("hlt");
    }
}

// Initialize PIT
void timer_init(uint32_t frequency) {
    timer_frequency = frequency;
    
    // Register handler
    register_interrupt_handler(32, timer_callback);
    
    // Calculate divisor
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 65535) divisor = 65535;
    if (divisor < 1) divisor = 1;
    
    // Send command byte: Channel 0, lobyte/hibyte, rate generator
    outb(PIT_COMMAND, 0x36);
    
    // Send divisor
    outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));
    
    // Unmask IRQ0
    irq_unmask(0);
    
    printf("[Timer] Initialized at %d Hz\n", frequency);
}
