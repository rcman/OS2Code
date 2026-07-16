// File: speaker.c
// PC Speaker driver implementation
// Uses PIT (Programmable Interval Timer) channel 2 and port 0x61

#include "types.h"
#include "speaker.h"

// External I/O functions
extern uint8_t inb(uint16_t port);
extern void outb(uint16_t port, uint8_t value);
extern uint32_t timer_get_ticks(void);
extern void printf(const char* format, ...);

// PC Speaker ports
#define PIT_CHANNEL_2   0x42    // PIT channel 2 data port
#define PIT_COMMAND     0x43    // PIT command port
#define SPEAKER_PORT    0x61    // PC speaker control port

// PIT base frequency
#define PIT_FREQUENCY   1193180  // Hz

// Initialize PC speaker
void speaker_init(void) {
    // Ensure speaker is off initially
    speaker_off();
}

// Turn speaker on at given frequency
void speaker_on(uint32_t frequency) {
    if (frequency == 0) {
        speaker_off();
        return;
    }

    // Calculate PIT divisor for desired frequency
    uint32_t divisor = PIT_FREQUENCY / frequency;
    if (divisor > 0xFFFF) divisor = 0xFFFF;  // Clamp to 16-bit max

    // Configure PIT channel 2 for square wave
    // Command: 10110110
    //   1011 = channel 2, access mode lobyte/hibyte, mode 3 (square wave)
    //   0110 = binary counting
    outb(PIT_COMMAND, 0xB6);

    // Send divisor (low byte, then high byte)
    outb(PIT_CHANNEL_2, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL_2, (uint8_t)((divisor >> 8) & 0xFF));

    // Enable speaker by setting bits 0 and 1 of port 0x61
    uint8_t tmp = inb(SPEAKER_PORT);
    if ((tmp & 0x3) != 0x3) {
        outb(SPEAKER_PORT, tmp | 0x3);
    }
}

// Turn speaker off
void speaker_off(void) {
    // Disable speaker by clearing bits 0 and 1 of port 0x61
    uint8_t tmp = inb(SPEAKER_PORT);
    outb(SPEAKER_PORT, tmp & 0xFC);
}

// Play a beep for specified duration
void speaker_beep(uint32_t frequency, uint32_t duration_ms) {
    if (frequency == 0 || duration_ms == 0) {
        return;  // Silent beep
    }

    // Turn on speaker at requested frequency
    speaker_on(frequency);

    // Convert milliseconds to timer ticks (100 Hz = 10ms per tick)
    uint32_t ticks_to_wait = (duration_ms + 9) / 10;  // Round up
    if (ticks_to_wait == 0) ticks_to_wait = 1;

    // Busy wait for duration (simple delay)
    uint32_t start_tick = timer_get_ticks();
    uint32_t end_tick = start_tick + ticks_to_wait;

    // Wait until enough ticks have passed
    while (timer_get_ticks() < end_tick) {
        __asm__ volatile("pause");  // Hint to CPU we're spinning
    }

    // Turn off speaker
    speaker_off();
}
