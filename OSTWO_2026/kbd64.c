// File: kbd64.c
// PS/2 keyboard driver for the 64-bit kernel (Phase B.6).
//
// IRQ1 handler translates scan-code set 1 into ASCII and pushes into a
// ring buffer; kbd64_getchar() blocks (HLT) until a key is available.

#include <stdint.h>

static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

#define KBUF 128
static volatile char ring[KBUF];
static volatile int head, tail;
static int shift, caps;

// Scan-code set 1 -> ASCII (unshifted / shifted), US layout
static const char map_lo[128] = {
    0,  27, '1','2','3','4','5','6','7','8','9','0','-','=','\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n', 0, 'a','s',
    'd','f','g','h','j','k','l',';','\'','`', 0,'\\','z','x','c','v',
    'b','n','m',',','.','/', 0, '*', 0, ' ',
};
static const char map_hi[128] = {
    0,  27, '!','@','#','$','%','^','&','*','(',')','_','+','\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n', 0, 'A','S',
    'D','F','G','H','J','K','L',':','"','~', 0, '|','Z','X','C','V',
    'B','N','M','<','>','?', 0, '*', 0, ' ',
};

// Called from the IRQ1 stub (irq64_kbd). EOI is sent by the stub.
void kbd64_handler(void) {
    uint8_t sc = inb(0x60);

    if (sc == 0x2A || sc == 0x36) { shift = 1; return; }   // shift down
    if (sc == 0xAA || sc == 0xB6) { shift = 0; return; }   // shift up
    if (sc == 0x3A) { caps ^= 1; return; }                 // caps lock
    if (sc & 0x80) return;                                  // key release
    if (sc >= 128) return;

    char c = (shift ^ caps) ? map_hi[sc] : map_lo[sc];
    // Caps affects letters only; for non-letters use plain shift state
    if (caps && !shift) {
        char l = map_lo[sc];
        if (l >= 'a' && l <= 'z') c = l - 32;
        else c = map_lo[sc];
    } else if (caps && shift) {
        char l = map_lo[sc];
        if (l >= 'a' && l <= 'z') c = l;      // caps+shift -> lowercase
        else c = map_hi[sc];
    }
    if (c == 0) return;

    int n = (head + 1) % KBUF;
    if (n != tail) {                                       // drop on overflow
        ring[head] = c;
        head = n;
    }
}

// Blocking read of one character.
char kbd64_getchar(void) {
    while (head == tail) {
        __asm__ volatile("sti; hlt");
    }
    char c = ring[tail];
    tail = (tail + 1) % KBUF;
    return c;
}

// Non-blocking: 1 if a key is buffered.
int kbd64_haskey(void) {
    return head != tail;
}
