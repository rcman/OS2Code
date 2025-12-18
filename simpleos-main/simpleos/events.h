// File: events.h
// Event system for input handling

#ifndef EVENTS_H
#define EVENTS_H

#include "types.h"

// Event types
#define EVENT_TYPE_NONE         0
#define EVENT_TYPE_KEY_DOWN     1
#define EVENT_TYPE_KEY_UP       2
#define EVENT_TYPE_MOUSE_MOVE   3
#define EVENT_TYPE_MOUSE_CLICK  4

// Event structure
typedef struct {
    uint8_t type;
    uint8_t data[8];
} input_event_t;

// Function prototypes
void push_event(input_event_t event);
input_event_t pop_event(void);
bool events_pending(void);
void events_clear(void);

#endif // EVENTS_H
