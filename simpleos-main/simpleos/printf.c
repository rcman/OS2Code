// File: printf.c
// Simple printf implementation for the kernel

#include "vga.h"
#include <stdarg.h>

// Convert integer to string
static void itoa(int value, char* buf, int base) {
    char* p = buf;
    char* p1, *p2;
    unsigned int ud = value;
    int divisor = base;
    
    // Handle negative for decimal
    if (base == 10 && value < 0) {
        *p++ = '-';
        buf++;
        ud = -value;
    }
    
    // Divide into digits
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder - 10 + 'a';
    } while (ud /= divisor);
    
    // Terminate
    *p = '\0';
    
    // Reverse
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }
}

// Convert unsigned integer to string
static void utoa(unsigned int value, char* buf, int base) {
    char* p = buf;
    char* p1, *p2;
    unsigned int ud = value;
    int divisor = base;
    
    // Divide into digits
    do {
        int remainder = ud % divisor;
        *p++ = (remainder < 10) ? remainder + '0' : remainder - 10 + 'a';
    } while (ud /= divisor);
    
    // Terminate
    *p = '\0';
    
    // Reverse
    p1 = buf;
    p2 = p - 1;
    while (p1 < p2) {
        char tmp = *p1;
        *p1++ = *p2;
        *p2-- = tmp;
    }
}

// External functions
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

// Initialize serial port COM1
void serial_init(void) {
    outb(0x3F8 + 1, 0x00);    // Disable interrupts
    outb(0x3F8 + 3, 0x80);    // Enable DLAB
    outb(0x3F8 + 0, 0x03);    // Divisor low byte (38400 baud)
    outb(0x3F8 + 1, 0x00);    // Divisor high byte
    outb(0x3F8 + 3, 0x03);    // 8 bits, no parity, one stop bit
    outb(0x3F8 + 2, 0xC7);    // Enable FIFO
    outb(0x3F8 + 4, 0x0B);    // IRQs enabled, RTS/DSR set
}

// Write character to serial port (COM1)
static void serial_putchar(char c) {
    // Wait for transmit buffer to be empty
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

// Write string to serial
static void serial_puts(const char* str) {
    while (*str) {
        if (*str == '\n') serial_putchar('\r');
        serial_putchar(*str++);
    }
}

// Printf implementation
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    char buf[32];
    
    while (*format) {
        if (*format == '%') {
            format++;
            switch (*format) {
                case 'd':
                case 'i':
                    itoa(va_arg(args, int), buf, 10);
                    vga_puts(buf);
                    serial_puts(buf);
                    break;
                    
                case 'u':
                    utoa(va_arg(args, unsigned int), buf, 10);
                    vga_puts(buf);
                    serial_puts(buf);
                    break;
                    
                case 'x':
                    utoa(va_arg(args, unsigned int), buf, 16);
                    vga_puts(buf);
                    serial_puts(buf);
                    break;
                    
                case 'X':
                    utoa(va_arg(args, unsigned int), buf, 16);
                    // Convert to uppercase
                    for (char* p = buf; *p; p++) {
                        if (*p >= 'a' && *p <= 'f') {
                            *p -= 32;
                        }
                    }
                    vga_puts(buf);
                    serial_puts(buf);
                    break;
                    
                case 'p':
                    vga_puts("0x");
                    serial_puts("0x");
                    utoa(va_arg(args, unsigned int), buf, 16);
                    vga_puts(buf);
                    serial_puts(buf);
                    break;
                    
                case 'c': {
                    char c = (char)va_arg(args, int);
                    vga_putchar(c);
                    serial_putchar(c);
                    break;
                }
                    
                case 's': {
                    const char* s = va_arg(args, const char*);
                    if (s) {
                        vga_puts(s);
                        serial_puts(s);
                    } else {
                        vga_puts("(null)");
                        serial_puts("(null)");
                    }
                    break;
                }
                    
                case '%':
                    vga_putchar('%');
                    serial_putchar('%');
                    break;
                    
                default:
                    vga_putchar('%');
                    serial_putchar('%');
                    vga_putchar(*format);
                    serial_putchar(*format);
                    break;
            }
        } else {
            vga_putchar(*format);
            if (*format == '\n') serial_putchar('\r');
            serial_putchar(*format);
        }
        format++;
    }
    
    va_end(args);
}
