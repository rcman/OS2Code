// File: printf.c
// Kernel printf with format-width, padding and 64-bit support.
//
// Supported: %d %i %u %x %X %p %c %s %%
// Flags:     '-' (left align), '0' (zero pad)
// Width:     decimal number (e.g. %8d, %04x)
// Length:    l, ll (e.g. %lu, %llu, %llx)
//
// The previous implementation ignored width/length modifiers, so
// every driver log using %02x / %04x / %llu printed the literal
// format string instead of the value (and left the variadic
// arguments misaligned for 64-bit values).

#include "vga.h"
#include <stdarg.h>

// External I/O functions
extern void outb(uint16_t port, uint8_t data);
extern uint8_t inb(uint16_t port);

// Optional GUI sink: when the GUI shell window is active, printf
// output is also forwarded there. Previously, everything printed via
// printf after the switch to graphics mode (exec status, ps/mem
// output, and user programs' own DosWrite output) went only to the
// invisible VGA text buffer and the serial port, so the on-screen
// shell showed nothing.
static void (*gui_sink)(char) = 0;

void printf_set_gui_sink(void (*fn)(char)) {
    gui_sink = fn;
}

// Write character to serial port (COM1)
static void serial_putchar(char c) {
    while ((inb(0x3F8 + 5) & 0x20) == 0);
    outb(0x3F8, c);
}

// Emit a character to both VGA and serial
static void emit_char(char c) {
    vga_putchar(c);
    if (gui_sink) gui_sink(c);
    if (c == '\n') serial_putchar('\r');
    serial_putchar(c);
}

static void emit_str(const char* s) {
    while (*s) emit_char(*s++);
}

// Convert unsigned 64-bit value to string in given base.
// Returns length. buf must hold at least 24 chars. (64-bit division
// is provided by libgcc, which the kernel now links against.)
static int u64toa(unsigned long long value, char* buf, int base, int uppercase) {
    const char* digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[24];
    int n = 0;

    do {
        tmp[n++] = digits[value % (unsigned)base];
        value /= (unsigned)base;
    } while (value && n < (int)sizeof(tmp));

    for (int i = 0; i < n; i++) {
        buf[i] = tmp[n - 1 - i];
    }
    buf[n] = '\0';
    return n;
}

// Emit a number/string with padding applied
static void emit_padded(const char* body, int body_len, int width,
                        int zero_pad, int left_align, int negative) {
    int pad = width - body_len - (negative ? 1 : 0);
    if (pad < 0) pad = 0;

    if (left_align) {
        if (negative) emit_char('-');
        emit_str(body);
        while (pad--) emit_char(' ');
    } else if (zero_pad) {
        // Sign comes before zero padding: -0042, not 00-42
        if (negative) emit_char('-');
        while (pad--) emit_char('0');
        emit_str(body);
    } else {
        while (pad--) emit_char(' ');
        if (negative) emit_char('-');
        emit_str(body);
    }
}

// Printf implementation
void printf(const char* format, ...) {
    va_list args;
    va_start(args, format);

    char buf[24];

    while (*format) {
        if (*format != '%') {
            emit_char(*format++);
            continue;
        }
        format++;  // skip '%'

        // Flags
        int left_align = 0, zero_pad = 0;
        for (;;) {
            if (*format == '-')      { left_align = 1; format++; }
            else if (*format == '0') { zero_pad = 1;   format++; }
            else break;
        }

        // Width
        int width = 0;
        while (*format >= '0' && *format <= '9') {
            width = width * 10 + (*format - '0');
            format++;
        }

        // Length modifiers
        int longs = 0;
        while (*format == 'l') { longs++; format++; }

        char spec = *format;
        if (spec == '\0') break;
        format++;

        switch (spec) {
            case 'd':
            case 'i': {
                long long v;
                if (longs >= 2)      v = va_arg(args, long long);
                else                 v = va_arg(args, int);   // int and long are both 32-bit
                int neg = (v < 0);
                unsigned long long uv = neg ? (unsigned long long)(-v) : (unsigned long long)v;
                int n = u64toa(uv, buf, 10, 0);
                emit_padded(buf, n, width, zero_pad, left_align, neg);
                break;
            }

            case 'u':
            case 'x':
            case 'X': {
                unsigned long long v;
                if (longs >= 2)      v = va_arg(args, unsigned long long);
                else                 v = va_arg(args, unsigned int);
                int base = (spec == 'u') ? 10 : 16;
                int n = u64toa(v, buf, base, spec == 'X');
                emit_padded(buf, n, width, zero_pad, left_align, 0);
                break;
            }

            case 'p': {
                unsigned int v = va_arg(args, unsigned int);
                int n = u64toa(v, buf, 16, 0);
                emit_str("0x");
                emit_padded(buf, n, 8, 1, 0, 0);  // pointers always %08x
                break;
            }

            case 'c': {
                char c = (char)va_arg(args, int);
                char s[2] = { c, '\0' };
                emit_padded(s, 1, width, 0, left_align, 0);
                break;
            }

            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                int n = 0;
                while (s[n]) n++;
                emit_padded(s, n, width, 0, left_align, 0);
                break;
            }

            case '%':
                emit_char('%');
                break;

            default:
                // Unknown specifier: print it literally
                emit_char('%');
                emit_char(spec);
                break;
        }
    }

    va_end(args);
}
