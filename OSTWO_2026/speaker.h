// File: speaker.h
// PC Speaker driver header

#ifndef SPEAKER_H
#define SPEAKER_H

#include "types.h"

// Initialize PC speaker
void speaker_init(void);

// Play a tone at given frequency (Hz) for given duration (milliseconds)
void speaker_beep(uint32_t frequency, uint32_t duration_ms);

// Turn speaker on/off
void speaker_on(uint32_t frequency);
void speaker_off(void);

#endif
