#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../kernel/types.h"

class Keyboard {
public:
    void init();
    char getchar();
    bool hasKey();
    
private:
    bool shift_pressed;
    bool ctrl_pressed;
    bool alt_pressed;
    bool caps_lock;
    
    char scanToChar(uint8_t scancode);
};

extern Keyboard keyboard;

#endif
