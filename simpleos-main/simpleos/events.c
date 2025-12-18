// File: events.c
// Event queue implementation

#include "events.h"

// Event queue
#define EVENT_QUEUE_SIZE 256
static input_event_t event_queue[EVENT_QUEUE_SIZE];
static volatile uint32_t queue_head = 0;
static volatile uint32_t queue_tail = 0;

// Push an event to the queue
void push_event(input_event_t event) {
    uint32_t next_head = (queue_head + 1) % EVENT_QUEUE_SIZE;
    
    // Check if queue is full
    if (next_head == queue_tail) {
        // Queue full, drop oldest event
        queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
    }
    
    event_queue[queue_head] = event;
    queue_head = next_head;
}

// Pop an event from the queue
input_event_t pop_event(void) {
    input_event_t event;
    
    // Check if queue is empty
    if (queue_head == queue_tail) {
        event.type = EVENT_TYPE_NONE;
        return event;
    }
    
    event = event_queue[queue_tail];
    queue_tail = (queue_tail + 1) % EVENT_QUEUE_SIZE;
    
    return event;
}

// Check if there are events pending
bool events_pending(void) {
    return queue_head != queue_tail;
}

// Clear all events
void events_clear(void) {
    queue_head = 0;
    queue_tail = 0;
}
