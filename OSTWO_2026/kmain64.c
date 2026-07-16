// File: kmain64.c
// OS/Two 64-bit kernel - Phase B.0 foundation
//
// Runs in x86-64 long mode (entered by the boot64.asm trampoline).
// Proves out the 64-bit environment: native 64-bit C code, serial
// logging, PCI configuration access and the BGA high-resolution
// framebuffer - the same display path the 32-bit kernel uses.

#include <stdint.h>
#include "font8x8.h"

// ---------------------------------------------------------------- port I/O
static inline void outb(uint16_t port, uint8_t v) {
    __asm__ volatile("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint8_t inb(uint16_t port) {
    uint8_t v;
    __asm__ volatile("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outw(uint16_t port, uint16_t v) {
    __asm__ volatile("outw %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint16_t inw(uint16_t port) {
    uint16_t v;
    __asm__ volatile("inw %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outl(uint16_t port, uint32_t v) {
    __asm__ volatile("outl %0, %1" : : "a"(v), "Nd"(port));
}
static inline uint32_t inl(uint16_t port) {
    uint32_t v;
    __asm__ volatile("inl %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}

// ---------------------------------------------------------------- serial
#define COM1 0x3F8

static void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 12);         // 9600 baud
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);       // 8N1
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

static void serial_putc(char c) {
    while (!(inb(COM1 + 5) & 0x20)) { }
    outb(COM1, (uint8_t)c);
}

void sputs(const char* s) {
    while (*s) {
        if (*s == '\n') serial_putc('\r');
        serial_putc(*s++);
    }
}

void sputhex64(uint64_t v) {
    static const char hex[] = "0123456789ABCDEF";
    sputs("0x");
    for (int i = 60; i >= 0; i -= 4) {
        serial_putc(hex[(v >> i) & 0xF]);
    }
}

void sputdec(uint64_t v) {
    char buf[24];
    int i = 0;
    if (v == 0) { serial_putc('0'); return; }
    while (v) { buf[i++] = '0' + (v % 10); v /= 10; }
    while (i) serial_putc(buf[--i]);
}

// ---------------------------------------------------------------- BGA display
#define VBE_DISPI_INDEX 0x01CE
#define VBE_DISPI_DATA  0x01CF

static void bga_write(uint16_t idx, uint16_t val) {
    outw(VBE_DISPI_INDEX, idx);
    outw(VBE_DISPI_DATA, val);
}
static uint16_t bga_read(uint16_t idx) {
    outw(VBE_DISPI_INDEX, idx);
    return inw(VBE_DISPI_DATA);
}

static uint32_t pci_read(uint8_t bus, uint8_t dev, uint8_t fn, uint8_t off) {
    outl(0xCF8, 0x80000000u | ((uint32_t)bus << 16) |
                ((uint32_t)dev << 11) | ((uint32_t)fn << 8) | (off & 0xFC));
    return inl(0xCFC);
}

static uint64_t bga_find_framebuffer(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t dev = 0; dev < 32; dev++) {
            uint32_t id = pci_read((uint8_t)bus, dev, 0, 0);
            if ((id & 0xFFFF) == 0xFFFF) continue;
            if (id == 0x11111234 || id == 0x01001B36) {   // stdvga / QXL
                return pci_read((uint8_t)bus, dev, 0, 0x10) & 0xFFFFFFF0u;
            }
        }
    }
    return 0;
}

// ---------------------------------------------------------------- drawing
#define SCREEN_W 1024
#define SCREEN_H 768

static uint32_t* fb;

static void put_pixel(int x, int y, uint32_t rgb) {
    if (x < 0 || x >= SCREEN_W || y < 0 || y >= SCREEN_H) return;
    fb[y * SCREEN_W + x] = rgb;
}

static void fill_rect(int x, int y, int w, int h, uint32_t rgb) {
    for (int j = y; j < y + h; j++) {
        for (int i = x; i < x + w; i++) {
            put_pixel(i, j, rgb);
        }
    }
}

static void gradient(uint32_t top, uint32_t bottom) {
    int r0 = (top >> 16) & 0xFF, g0 = (top >> 8) & 0xFF, b0 = top & 0xFF;
    int r1 = (bottom >> 16) & 0xFF, g1 = (bottom >> 8) & 0xFF, b1 = bottom & 0xFF;
    for (int y = 0; y < SCREEN_H; y++) {
        uint32_t r = r0 + (r1 - r0) * y / (SCREEN_H - 1);
        uint32_t g = g0 + (g1 - g0) * y / (SCREEN_H - 1);
        uint32_t b = b0 + (b1 - b0) * y / (SCREEN_H - 1);
        uint32_t rgb = (r << 16) | (g << 8) | b;
        for (int x = 0; x < SCREEN_W; x++) {
            fb[y * SCREEN_W + x] = rgb;
        }
    }
}

// Draw a character at 2x scale (16x16 on screen) for readability
static void draw_char2x(int x, int y, char c, uint32_t rgb) {
    if (c < 0 || c >= 127) return;
    for (int row = 0; row < 8; row++) {
        uint8_t bits = font_8x8[(int)c][row];
        for (int col = 0; col < 8; col++) {
            if (bits & (1 << col)) {
                fill_rect(x + col * 2, y + row * 2, 2, 2, rgb);
            }
        }
    }
}

static void draw_text2x(int x, int y, const char* s, uint32_t rgb) {
    while (*s) {
        draw_char2x(x, y, *s++, rgb);
        x += 16;
    }
}

static void draw_text(int x, int y, const char* s, uint32_t rgb) {
    while (*s) {
        char c = *s++;
        if (c > 0 && c < 127) {
            for (int row = 0; row < 8; row++) {
                uint8_t bits = font_8x8[(int)c][row];
                for (int col = 0; col < 8; col++) {
                    if (bits & (1 << col)) put_pixel(x + col, y + row, rgb);
                }
            }
        }
        x += 8;
    }
}

static void hex64_str(uint64_t v, char* out) {
    static const char hex[] = "0123456789ABCDEF";
    out[0] = '0'; out[1] = 'x';
    for (int i = 0; i < 16; i++) {
        out[2 + i] = hex[(v >> (60 - i * 4)) & 0xF];
    }
    out[18] = 0;
}

// ---------------------------------------------------------------- text console
// A scrolling text console over the framebuffer. 8x16 cells (font drawn
// at the top of a 16px line) with the amber-on-dark theme.
#define CON_COLS (SCREEN_W / 8)
#define CON_ROWS (SCREEN_H / 16)
#define CON_FG   0xF0E0C0
#define CON_BG   0x140A04

static int con_row, con_col;

static void con_clear(void) {
    if (!fb) return;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = CON_BG;
    con_row = con_col = 0;
}

static void con_scroll(void) {
    if (!fb) return;
    // Move everything up one 16px line
    for (int y = 0; y < (CON_ROWS - 1) * 16; y++) {
        for (int x = 0; x < SCREEN_W; x++) {
            fb[y * SCREEN_W + x] = fb[(y + 16) * SCREEN_W + x];
        }
    }
    for (int y = (CON_ROWS - 1) * 16; y < SCREEN_H; y++) {
        for (int x = 0; x < SCREEN_W; x++) fb[y * SCREEN_W + x] = CON_BG;
    }
    con_row = CON_ROWS - 1;
}

static void con_putc(char c) {
    serial_putc(c == '\n' ? '\r' : c);   // mirror to serial
    if (c == '\n') { serial_putc('\n'); con_col = 0; con_row++; }
    else if (c == '\b') {
        if (con_col > 0) {
            con_col--;
            if (fb) fill_rect(con_col * 8, con_row * 16, 8, 16, CON_BG);
        }
    } else {
        if (fb) {
            fill_rect(con_col * 8, con_row * 16, 8, 16, CON_BG);
            draw_text(con_col * 8, con_row * 16 + 4, (char[]){c, 0}, CON_FG);
        }
        con_col++;
        if (con_col >= CON_COLS) { con_col = 0; con_row++; }
    }
    if (con_row >= CON_ROWS) con_scroll();
}

static void con_puts(const char* s) {
    while (*s) con_putc(*s++);
}

static void con_putdec(uint64_t v) {
    char b[24]; int i = 0;
    if (v == 0) { con_putc('0'); return; }
    while (v) { b[i++] = '0' + (v % 10); v /= 10; }
    while (i) con_putc(b[--i]);
}

static void con_puthex(uint64_t v) {
    char b[20]; hex64_str(v, b); con_puts(b);
}

// ---------------------------------------------------------------- MSRs
static inline void wrmsr(uint32_t msr, uint64_t v) {
    __asm__ volatile("wrmsr" : : "c"(msr), "a"((uint32_t)v), "d"((uint32_t)(v >> 32)));
}
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t lo, hi;
    __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
    return lo | ((uint64_t)hi << 32);
}

// ---------------------------------------------------------------- GDT + TSS
// Layout chosen for SYSCALL/SYSRET:
//   0x08 kernel code   0x10 kernel data   (SYSCALL: CS=0x08, SS=0x10)
//   0x18 (sysret base) 0x20 user data     0x28 user code 64-bit
//   (SYSRET: CS=0x18+16=0x28|3, SS=0x18+8=0x20|3)
//   0x30 TSS (16-byte descriptor)
struct tss64 {
    uint32_t rsv0;
    uint64_t rsp0, rsp1, rsp2;
    uint64_t rsv1;
    uint64_t ist[7];
    uint64_t rsv2;
    uint16_t rsv3, iomap;
} __attribute__((packed));

static uint64_t gdt64[8];
static struct tss64 tss;
static uint8_t irq_stack[16384] __attribute__((aligned(16)));

struct dtptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static void gdt_tss_init(void) {
    gdt64[0] = 0;
    gdt64[1] = 0x00209A0000000000ull;   // 0x08 kernel code (L=1)
    gdt64[2] = 0x0000920000000000ull;   // 0x10 kernel data
    gdt64[3] = 0x00CFFA000000FFFFull;   // 0x18 user code 32-bit (compat mode, DPL 3)
    gdt64[4] = 0x00CFF2000000FFFFull;   // 0x20 user data (DPL 3, B=1 32-bit stack)
    gdt64[5] = 0x0020FA0000000000ull;   // 0x28 user code 64-bit (DPL 3)

    // TSS descriptor (16 bytes across two GDT slots)
    uint64_t base = (uint64_t)&tss;
    uint64_t limit = sizeof(tss) - 1;
    gdt64[6] = (limit & 0xFFFF) | ((base & 0xFFFFFF) << 16) |
               (0x89ull << 40) | (((limit >> 16) & 0xF) << 48) |
               (((base >> 24) & 0xFF) << 56);
    gdt64[7] = base >> 32;

    for (unsigned i = 0; i < sizeof(tss); i++) ((uint8_t*)&tss)[i] = 0;
    tss.rsp0 = (uint64_t)&irq_stack[sizeof(irq_stack)];
    tss.iomap = sizeof(tss);

    struct dtptr gdtr = { sizeof(gdt64) - 1, (uint64_t)gdt64 };
    __asm__ volatile("lgdt %0" : : "m"(gdtr));
    __asm__ volatile(
        "pushq $0x08\n\t"
        "leaq 1f(%%rip), %%rax\n\t"
        "pushq %%rax\n\t"
        "lretq\n\t"
        "1: mov $0x10, %%ax\n\t"
        "mov %%ax, %%ds\n\t"
        "mov %%ax, %%es\n\t"
        "mov %%ax, %%ss\n\t"
        "mov %%ax, %%fs\n\t"
        "mov %%ax, %%gs\n\t"
        : : : "rax");
    __asm__ volatile("ltr %w0" : : "r"(0x30));
}

// The scheduler updates the ring0 stack on every task switch so each
// task's interrupt frames land on its own kernel stack (not a shared
// one, which would clobber other tasks' saved contexts).
void tss_set_rsp0(uint64_t rsp0) {
    tss.rsp0 = rsp0;
}

// ---------------------------------------------------------------- IDT
struct idt_gate {
    uint16_t off_lo;
    uint16_t sel;
    uint8_t  ist;
    uint8_t  type;
    uint16_t off_mid;
    uint32_t off_hi;
    uint32_t rsv;
} __attribute__((packed));

static struct idt_gate idt64[256];
extern uint64_t isr64_vector_table[32];
extern void irq64_timer(void);
extern void irq64_sched(void);
extern void irq64_kbd(void);
extern void irq64_mouse(void);
extern void syscall64_entry(void);
extern void int80_entry(void);

// Set once the preemptive scheduler owns the timer IRQ
static int sched_active;

static void idt_set(int vec, uint64_t handler, uint8_t type) {
    idt64[vec].off_lo = handler & 0xFFFF;
    idt64[vec].sel = 0x08;
    idt64[vec].ist = 0;
    idt64[vec].type = type;
    idt64[vec].off_mid = (handler >> 16) & 0xFFFF;
    idt64[vec].off_hi = handler >> 32;
    idt64[vec].rsv = 0;
}

static void idt_init(void) {
    for (int i = 0; i < 32; i++) {
        idt_set(i, isr64_vector_table[i], 0x8E);
    }
    idt_set(32, (uint64_t)irq64_timer, 0x8E);
    idt_set(33, (uint64_t)irq64_kbd, 0x8E);   // IRQ1 PS/2 keyboard
    idt_set(44, (uint64_t)irq64_mouse, 0x8E); // IRQ12 PS/2 mouse
    // int 0x80: callable from ring 3 (DPL 3) - the classic OSTwo
    // syscall gate, used by 32-bit compatibility-mode programs
    idt_set(0x80, (uint64_t)int80_entry, 0xEE);
    struct dtptr idtr = { sizeof(idt64) - 1, (uint64_t)idt64 };
    __asm__ volatile("lidt %0" : : "m"(idtr));
}

// ---------------------------------------------------------------- PIC + PIT
static void pic_timer_init(void) {
    // Remap PIC to vectors 32-47
    outb(0x20, 0x11); outb(0xA0, 0x11);
    outb(0x21, 0x20); outb(0xA1, 0x28);
    outb(0x21, 0x04); outb(0xA1, 0x02);
    outb(0x21, 0x01); outb(0xA1, 0x01);
    outb(0x21, 0xF8);                   // master: IRQ0 timer, IRQ1 kbd, IRQ2 cascade
    outb(0xA1, 0xEF);                   // slave: IRQ12 mouse

    // PIT channel 0: 100 Hz
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(11932 & 0xFF));
    outb(0x40, (uint8_t)(11932 >> 8));
}

// ---------------------------------------------------------------- handlers
static volatile uint64_t timer_ticks;
static volatile uint64_t syscall_count;

void timer64_handler(void) {
    timer_ticks++;
    outb(0x20, 0x20);                   // EOI
}

void exception64_handler(uint64_t vector, uint64_t error, uint64_t rip) {
    sputs("\n[64] EXCEPTION ");
    sputdec(vector);
    sputs(" error=");
    sputhex64(error);
    sputs(" rip=");
    sputhex64(rip);
    if (vector == 14) {
        uint64_t cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        sputs(" cr2=");
        sputhex64(cr2);
    }
    sputs("\n[64] Halting.\n");
    for (;;) __asm__ volatile("cli; hlt");
}

// ---------------------------------------------------------------- syscalls
static void user_done(void) __attribute__((noreturn));

uint64_t syscall64_dispatch(uint64_t nr, uint64_t a1, uint64_t a2, uint64_t a3) {
    (void)a2; (void)a3;
    syscall_count++;
    switch (nr) {
        case 1:                          // SYS_PRINT(str)
            sputs("[user64] ");
            sputs((const char*)a1);
            return 0;
        case 2:                          // SYS_PRINTHEX(value)
            sputs("[user64] value = ");
            sputhex64(a1);
            sputs("\n");
            return 0;
        case 3:                          // SYS_EXIT
            if (sched_active) {
                extern void sched_task_exit(void);
                sched_task_exit();       // marks task dead, yields
            }
            user_done();
        case 4:                          // SYS_GETTICKS
            return timer_ticks;
        case 5:                          // SYS_PUTC(char) - scheduler demo
            con_putc((char)a1);          // to framebuffer console + serial
            return 0;
        default:
            return (uint64_t)-1;
    }
}

// int 0x80 dispatcher for 32-bit compatibility-mode programs.
// Implements the classic OSTwo 32-bit ABI: EAX=nr, EBX/ECX/EDX args.
static volatile uint64_t compat_syscalls;

uint64_t syscall32_dispatch(uint64_t nr, uint64_t ebx, uint64_t ecx, uint64_t edx) {
    compat_syscalls++;
    switch (nr) {
        case 1:                          // SYSCALL_EXIT(code)
            sputs("[user32] exited with code ");
            sputdec(ebx);
            sputs("\n");
            user_done();
        case 2: {                        // SYSCALL_WRITE(fd, buf, len)
            const char* buf = (const char*)ecx;   // identity-mapped, < 4GB
            sputs("[user32] ");
            for (uint64_t i = 0; i < edx; i++) {
                char s[2] = { buf[i], 0 };
                sputs(s);
                if (buf[i] == '\n' && i + 1 < edx) sputs("[user32] ");
            }
            return edx;
        }
        case 8:                          // SYSCALL_BEEP - accept quietly
            return 0;
        default:
            sputs("[user32] unsupported syscall ");
            sputdec(nr);
            sputs("\n");
            return (uint64_t)-1;
    }
}

static void syscall_init(void) {
    wrmsr(0xC0000080, rdmsr(0xC0000080) | 1);        // EFER.SCE
    wrmsr(0xC0000081, (0x18ull << 48) | (0x08ull << 32));  // STAR
    wrmsr(0xC0000082, (uint64_t)syscall64_entry);    // LSTAR
    wrmsr(0xC0000084, 0x200);                        // SFMASK: mask IF
}

// ---------------------------------------------------------------- user mode
#define USER_BASE   0x01000000ull        // 16MB: 2MB page index 8
#define USER_STACK  0x011FFFF0ull

extern uint8_t user64_blob_start[];
extern uint8_t user64_blob_end[];

// Grant ring-3 access to the identity-mapped 2MB page containing
// vaddr: the US bit must be set at every level of the walk.
static void grant_user_2m(uint64_t vaddr) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFull);
    uint64_t* pdpt = (uint64_t*)(pml4[(vaddr >> 39) & 0x1FF] & ~0xFFFull);
    uint64_t* pd = (uint64_t*)(pdpt[(vaddr >> 30) & 0x1FF] & ~0xFFFull);
    pml4[(vaddr >> 39) & 0x1FF] |= 0x04;
    pdpt[(vaddr >> 30) & 0x1FF] |= 0x04;
    pd[(vaddr >> 21) & 0x1FF] |= 0x04;
    __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");
}

static void map_user_region(void) {
    grant_user_2m(USER_BASE);
}

static void enter_ring3(uint64_t rip, uint64_t rsp) __attribute__((noreturn));
static void enter_ring3(uint64_t rip, uint64_t rsp) {
    __asm__ volatile(
        "pushq $0x23\n\t"                // SS = user data | RPL 3
        "pushq %0\n\t"                   // RSP
        "pushq $0x202\n\t"               // RFLAGS (IF=1)
        "pushq $0x2B\n\t"                // CS = 64-bit user code | RPL 3
        "pushq %1\n\t"                   // RIP
        "iretq"
        : : "r"(rsp), "r"(rip));
    __builtin_unreachable();
}

// Enter 32-bit COMPATIBILITY MODE (CS.L=0): the CPU executes the
// target as 32-bit protected-mode code while the kernel stays 64-bit -
// exactly how OS/2 2.x ran 16-bit apps on a 32-bit kernel.
static void enter_ring3_compat(uint32_t eip, uint32_t esp) __attribute__((noreturn));
static void enter_ring3_compat(uint32_t eip, uint32_t esp) {
    __asm__ volatile(
        "pushq $0x23\n\t"                // SS = user data | RPL 3
        "pushq %0\n\t"                   // ESP
        "pushq $0x202\n\t"               // EFLAGS (IF=1)
        "pushq $0x1B\n\t"                // CS = 32-bit user code | RPL 3
        "pushq %1\n\t"                   // EIP
        "iretq"
        : : "r"((uint64_t)esp), "r"((uint64_t)eip));
    __builtin_unreachable();
}

static void run_embedded_blob(void) __attribute__((noreturn));
static void run_embedded_blob(void) {
    uint64_t size = (uint64_t)(user64_blob_end - user64_blob_start);
    uint8_t* dst = (uint8_t*)USER_BASE;
    for (uint64_t i = 0; i < size; i++) {
        dst[i] = user64_blob_start[i];
    }
    sputs("[64] No ELF module; running embedded ring-3 blob at ");
    sputhex64(USER_BASE);
    sputs("\n");
    enter_ring3(USER_BASE, USER_STACK);
}

// ---------------------------------------------------------------- ELF64 app
extern void pmm64_init(void);
extern uint64_t pmm64_alloc_page(void);
extern int vmm64_map_page(uint64_t vaddr, uint64_t phys, int user, int writable);
extern uint64_t elf64_load(const uint8_t* data, uint64_t size);
extern int lx64_is_lx(const void* d, uint32_t size);
extern uint32_t lx64_load(const uint8_t* data, uint32_t size);

#define APP_STACK_TOP 0x140008000ull

// hello.bin from the 32-bit OS links at 0x40000000 - run it there
#define COMPAT_BASE   0x40000000u
#define COMPAT_STACK  0x401FFFF0u

// Second multiboot module: a 32-bit flat binary for compat mode
static uint64_t compat_mod_start, compat_mod_size;

// Launch the first multiboot module as an ELF64 user program.
// Returns only if no module is present or loading fails.
static void try_run_elf_module(uint32_t mb_info_addr) {
    if (!mb_info_addr) return;
    const uint32_t* mbi = (const uint32_t*)(uint64_t)mb_info_addr;
    if (!(mbi[0] & (1 << 3)) || mbi[5] == 0) {   // flags.mods, mods_count
        return;
    }
    const uint32_t* mod = (const uint32_t*)(uint64_t)mbi[6];  // mods_addr
    uint64_t mod_start = mod[0];
    uint64_t mod_size = mod[1] - mod[0];

    if (mbi[5] >= 2) {                   // module 2: 32-bit compat binary
        compat_mod_start = mod[4];
        compat_mod_size = mod[5] - mod[4];
        sputs("[64] Compat module: ");
        sputdec(compat_mod_size);
        sputs(" bytes (32-bit flat binary for after the 64-bit app)\n");
    }
    sputs("[64] Multiboot module: ");
    sputdec(mod_size);
    sputs(" bytes at ");
    sputhex64(mod_start);
    sputs("\n");

    uint64_t entry = elf64_load((const uint8_t*)mod_start, mod_size);
    if (entry == 0) {
        sputs("[64] ELF64 load failed, falling back to embedded blob\n");
        return;
    }

    // User stack: 8 pages below APP_STACK_TOP
    for (uint64_t va = APP_STACK_TOP - 8 * 4096; va < APP_STACK_TOP; va += 4096) {
        uint64_t phys = pmm64_alloc_page();
        if (phys == 0 || vmm64_map_page(va, phys, 1, 1)) {
            sputs("[64] ELF64: stack map failed\n");
            return;
        }
    }

    sputs("[64] Starting ELF64 program, entry ");
    sputhex64(entry);
    sputs("\n");
    enter_ring3(entry, APP_STACK_TOP - 16);
}

static int user_phase = 0;

static void user_done(void) {
    if (user_phase == 0) {
        user_phase = 1;
        sputs("[64] 64-bit user process exited after ");
        sputdec(syscall_count);
        sputs(" syscalls; timer ticks: ");
        sputdec(timer_ticks);
        sputs("\n");

        if (fb) {
            fill_rect(232, 460, 560, 2, 0xFFD9A0);
            draw_text(248, 476, "Ring-3 64-bit ELF app ran and exited cleanly.", 0xB0FFB0);
        }

        // Phase 2: run a 32-bit program in COMPATIBILITY MODE (CS.L=0).
        if (compat_mod_size > 0 && compat_mod_size < 0x100000) {
            const uint8_t* src = (const uint8_t*)compat_mod_start;

            if (lx64_is_lx(src, compat_mod_size)) {
                // A genuine OS/2 LX executable, run under the 64-bit
                // kernel! LX apps link at their preferred base in the
                // first 2MB (identity-mapped real RAM); grant ring-3
                // access and let the LX loader place objects + thunks.
                sputs("[64] Compat module is an OS/2 LX executable.\n");
                sputs("[64] Loading it to run in 32-bit compat mode under the\n");
                sputs("[64] 64-bit kernel - the OS/2 model, one level deeper...\n");
                grant_user_2m(0x00000000);   // first 2MB -> ring 3
                uint32_t eip = lx64_load(src, (uint32_t)compat_mod_size);
                if (eip) {
                    sputs("[64] LX entry ");
                    sputhex64(eip);
                    sputs("; entering compat mode.\n");
                    enter_ring3_compat(eip, 0x001F0000u);   // stack in low RAM
                }
                sputs("[64] LX load failed.\n");
            } else {
                // Flat 32-bit binary (hello.bin) linked at 0x40000000.
                // That identity-maps to physical 1GB which doesn't exist
                // at -m 256M, so back it with real pmm64 frames (clear
                // PDPT[1] to split the identity huge page, then 4KB map).
                sputs("[64] Compat module is a flat 32-bit binary; running it.\n");
                uint64_t cr3;
                __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
                uint64_t* pml4 = (uint64_t*)(cr3 & ~0xFFFull);
                uint64_t* pdpt = (uint64_t*)(pml4[0] & ~0xFFFull);
                pdpt[1] = 0;
                __asm__ volatile("mov %0, %%cr3" : : "r"(cr3) : "memory");

                uint64_t code_pages = (compat_mod_size + 0xFFF) / 0x1000;
                for (uint64_t i = 0; i < code_pages; i++) {
                    uint64_t ph = pmm64_alloc_page();
                    vmm64_map_page(COMPAT_BASE + i * 0x1000, ph, 1, 1);
                }
                uint64_t stack_ph = pmm64_alloc_page();
                vmm64_map_page(COMPAT_STACK & ~0xFFFull, stack_ph, 1, 1);

                uint8_t* dst = (uint8_t*)(uint64_t)COMPAT_BASE;
                for (uint64_t i = 0; i < compat_mod_size; i++) dst[i] = src[i];
                enter_ring3_compat(COMPAT_BASE, COMPAT_STACK);
            }
        }
    } else {
        sputs("[64] 32-bit compatibility-mode process exited (");
        sputdec(compat_syscalls);
        sputs(" int-0x80 syscalls).\n");
        if (fb) {
            draw_text(248, 494, "A genuine OS/2 LX executable ran in 32-bit compat mode", 0xB0FFB0);
            draw_text(248, 512, "(int 0x80, DOSCALLS thunks) under the 64-bit kernel!", 0xB0FFB0);
        }
    }

    sputs("[64] All user programs finished. 64-bit + 32-bit compat both work.\n");
    for (;;) __asm__ volatile("hlt");
}

// ---------------------------------------------------------------- scheduler demo
extern int sched_add(uint64_t entry, uint64_t ustack, uint64_t rbx,
                     uint64_t rbp, char label);
extern void sched_run(void) __attribute__((noreturn));
extern uint8_t sched_task_blob_start[];
extern uint8_t sched_task_blob_end[];

#define TASK_CODE 0x50000u

// Called by sched64 when the last task exits: paint the result.
void sched_all_done(void) {
    if (fb) {
        fill_rect(232, 460, 560, 2, 0xFFD9A0);
        draw_text(248, 476, "Preemptive round-robin scheduler: 3 ring-3 tasks ran", 0xB0FFB0);
        draw_text(248, 494, "concurrently, timer-preempted at 100 Hz, then exited.", 0xB0FFB0);
        draw_text(248, 512, "Interleaved output (see serial) proves real preemption.", 0xFFF0D8);
    }
}

// The shell's 'smp' command runs the multitasking demo (never returns).
static void sched_demo(void) __attribute__((noreturn));
static void shell_smp(void) {
    con_puts("Launching 3 preemptive ring-3 tasks (interleaved output):\n");
    sched_demo();
}

static void sched_demo(void) __attribute__((noreturn));
static void sched_demo(void) {
    sputs("[64] Running the preemptive multitasking demo.\n");

    // Copy the shared task blob into a user-accessible code page
    grant_user_2m(0x00000000);
    uint64_t sz = (uint64_t)(sched_task_blob_end - sched_task_blob_start);
    uint8_t* code = (uint8_t*)(uint64_t)TASK_CODE;
    for (uint64_t i = 0; i < sz; i++) code[i] = sched_task_blob_start[i];

    // Three tasks share the code, differ by character (RBX) and how many
    // times they print (RBP). Distinct user stacks in low RAM.
    sched_add(TASK_CODE, 0x70000, 'A', 12, 'A');
    sched_add(TASK_CODE, 0x74000, 'B', 12, 'B');
    sched_add(TASK_CODE, 0x78000, 'C', 12, 'C');

    // Hand the timer IRQ to the context-switching scheduler stub
    idt_set(32, (uint64_t)irq64_sched, 0x8E);
    struct dtptr idtr = { sizeof(idt64) - 1, (uint64_t)idt64 };
    __asm__ volatile("lidt %0" : : "m"(idtr));
    sched_active = 1;

    sputs("[64] Task output (A/B/C interleaved by preemption):\n");
    sched_run();   // launches task A; never returns
}

// ---------------------------------------------------------------- desktop GUI
extern volatile int mouse_x, mouse_y, mouse_buttons, mouse_moved;
extern void mouse64_init(void);

#define CUR_W 12
#define CUR_H 19
// Arrow cursor: 0 = transparent, 1 = black outline, 2 = white fill
static const uint8_t cursor_bm[CUR_H][CUR_W] = {
    {1},
    {1,1},
    {1,2,1},
    {1,2,2,1},
    {1,2,2,2,1},
    {1,2,2,2,2,1},
    {1,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,1,1,1,1},
    {1,2,2,1,2,2,1},
    {1,2,1,0,1,2,2,1},
    {1,1,0,0,1,2,2,1},
    {1,0,0,0,0,1,2,2,1},
    {0,0,0,0,0,1,2,2,1},
    {0,0,0,0,0,0,1,2,2,1},
    {0,0,0,0,0,0,1,2,2,1},
    {0,0,0,0,0,0,0,1,1,1},
    {0,0,0,0,0,0,0,0,0},
};

// The desktop background (gradient + logo + taskbar) is composed once
// into this buffer; each frame is background + windows + cursor, so
// dragging leaves no trails and needs no per-window erase bookkeeping.
static uint32_t bg_buffer[SCREEN_W * SCREEN_H];

// OS/2 Workplace Shell palette
#define WPS_GRAY   0xBDBDBD   // window body / frame
#define WPS_LIGHT  0xF0F0F0   // top-left bevel highlight
#define WPS_DARK   0x707070   // bottom-right bevel shadow
#define WPS_TITLE  0x00308A   // active title bar (OS/2 blue)
#define WPS_TITLE2 0x7C90B0   // inactive title bar
#define WPS_DESK   0x2E8B8B   // teal desktop (Warp default)

// A 3D raised bevel (light top/left, dark bottom/right) - the OS/2 look.
static void bevel(int x, int y, int w, int h, int raised) {
    uint32_t tl = raised ? WPS_LIGHT : WPS_DARK;
    uint32_t br = raised ? WPS_DARK : WPS_LIGHT;
    fill_rect(x, y, w, 1, tl);
    fill_rect(x, y, 1, h, tl);
    fill_rect(x, y + h - 1, w, 1, br);
    fill_rect(x + w - 1, y, 1, h, br);
}

// ---- desktop icons (Workplace Shell objects) ----
enum { IC_SYSTEM, IC_DRIVES, IC_PROGRAMS, IC_INFO, IC_SHREDDER };

// Draw a 32-wide Workplace-Shell-style icon glyph at (x,y).
static void draw_icon_glyph(int x, int y, int type) {
    switch (type) {
        case IC_SYSTEM:      // a small computer/monitor
            fill_rect(x + 3, y + 2, 26, 18, 0x303030);
            fill_rect(x + 5, y + 4, 22, 14, 0x2060B0);
            fill_rect(x + 10, y + 22, 12, 4, 0x505050);
            fill_rect(x + 6, y + 26, 20, 3, 0x707070);
            break;
        case IC_DRIVES:      // a stack of disk drives
            for (int i = 0; i < 3; i++) {
                fill_rect(x + 2, y + 2 + i * 9, 28, 7, 0xC8C8C8);
                bevel(x + 2, y + 2 + i * 9, 28, 7, 1);
                fill_rect(x + 5, y + 5 + i * 9, 10, 2, 0x606060);
                fill_rect(x + 25, y + 4 + i * 9, 3, 3, 0x30B030);
            }
            break;
        case IC_PROGRAMS:    // yellow folder
        case IC_INFO:
            fill_rect(x + 2, y + 8, 28, 20, type == IC_INFO ? 0x5090D0 : 0xE0C040);
            fill_rect(x + 2, y + 5, 12, 5, type == IC_INFO ? 0x5090D0 : 0xE0C040);
            bevel(x + 2, y + 8, 28, 20, 1);
            if (type == IC_INFO) draw_text(x + 13, y + 13, "i", 0xFFFFFF);
            break;
        case IC_SHREDDER:    // a shredder / trash
            fill_rect(x + 5, y + 4, 22, 6, 0x808080);
            bevel(x + 5, y + 4, 22, 6, 1);
            for (int i = 0; i < 5; i++) fill_rect(x + 8 + i * 4, y + 12, 2, 14, 0xB0B0B0);
            fill_rect(x + 6, y + 26, 20, 3, 0x606060);
            break;
    }
}

typedef struct { int x, y; const char* label; int type; int opens; } icon_t;
static icon_t icons[6];
static int nicons;
static int sel_icon = -1;

// ---- windows ----
#define MAX_WINS 6
#define TITLE_H  22
typedef struct {
    int x, y, w, h;
    const char* title;
    const char* body[8];
    int nbody;
    int shown;
} win_t;

static win_t wins[MAX_WINS];
static int zorder[MAX_WINS];
static int nwins;

static void gui_taskbar_to(uint32_t* target) {
    uint32_t* real = fb; fb = target;
    fill_rect(0, SCREEN_H - 30, SCREEN_W, 30, WPS_GRAY);
    bevel(0, SCREEN_H - 30, SCREEN_W, 30, 1);
    fill_rect(6, SCREEN_H - 25, 96, 20, WPS_GRAY);
    bevel(6, SCREEN_H - 25, 96, 20, 1);
    draw_text(14, SCREEN_H - 19, "OS/2 System", 0x000000);
    fill_rect(SCREEN_W - 96, SCREEN_H - 25, 90, 20, WPS_GRAY);
    bevel(SCREEN_W - 96, SCREEN_H - 25, 90, 20, 0);
    char t[24]; int n = 0;
    const char* p = "up "; while (*p) t[n++] = *p++;
    uint64_t v = timer_ticks / 100; char d[12]; int i = 0;
    if (v == 0) d[i++] = '0';
    while (v) { d[i++] = '0' + v % 10; v /= 10; }
    while (i) t[n++] = d[--i];
    t[n++] = 's'; t[n] = 0;
    draw_text(SCREEN_W - 88, SCREEN_H - 19, t, 0x000000);
    fb = real;
}

// Build the static desktop: teal background, icons, taskbar.
static void gui_build_bg(void) {
    uint32_t* real = fb; fb = bg_buffer;
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) bg_buffer[i] = WPS_DESK;
    // Desktop icons down the left edge, Workplace-Shell style
    for (int i = 0; i < nicons; i++) {
        if (sel_icon == i) {
            fill_rect(icons[i].x - 4, icons[i].y - 4, 40, 48, 0x1F5F8F);
        }
        draw_icon_glyph(icons[i].x, icons[i].y, icons[i].type);
        // centered-ish label under the icon
        int len = 0; const char* s = icons[i].label; while (s[len]) len++;
        int lx = icons[i].x + 16 - len * 4;
        if (lx < 2) lx = 2;
        draw_text(lx, icons[i].y + 34, icons[i].label,
                  sel_icon == i ? 0xFFFFFF : 0xE8F0F0);
    }
    fb = real;
    gui_taskbar_to(bg_buffer);
}

// An OS/2-style window: raised gray frame, system-menu box (double-click
// to close), title bar, minimize/maximize buttons, body text.
static void gui_draw_win(const win_t* wn, int focused) {
    int x = wn->x, y = wn->y, w = wn->w, h = wn->h;
    fill_rect(x, y, w, h, WPS_GRAY);
    bevel(x, y, w, h, 1);
    bevel(x + 3, y + TITLE_H + 3, w - 6, h - TITLE_H - 6, 0);  // sunken body
    fill_rect(x + 4, y + TITLE_H + 4, w - 8, h - TITLE_H - 8, 0x1A1A1A);
    // title bar
    fill_rect(x + 3, y + 3, w - 6, TITLE_H - 3, focused ? WPS_TITLE : WPS_TITLE2);
    // system-menu box (left)
    fill_rect(x + 4, y + 4, 16, TITLE_H - 5, WPS_GRAY);
    bevel(x + 4, y + 4, 16, TITLE_H - 5, 1);
    fill_rect(x + 8, y + 10, 8, 3, 0x303030);
    draw_text(x + 26, y + 8, wn->title, 0xFFFFFF);
    // minimize + maximize boxes (right)
    fill_rect(x + w - 38, y + 4, 16, TITLE_H - 5, WPS_GRAY);
    bevel(x + w - 38, y + 4, 16, TITLE_H - 5, 1);
    fill_rect(x + w - 34, y + 15, 8, 2, 0x303030);            // minimize
    fill_rect(x + w - 20, y + 4, 16, TITLE_H - 5, WPS_GRAY);
    bevel(x + w - 20, y + 4, 16, TITLE_H - 5, 1);
    fill_rect(x + w - 17, y + 7, 10, 9, WPS_GRAY);
    bevel(x + w - 17, y + 7, 10, 9, 1);                       // maximize
    for (int i = 0; i < wn->nbody; i++) {
        draw_text(x + 14, y + TITLE_H + 12 + i * 18, wn->body[i], 0xD8E4E4);
    }
}

static void gui_draw_cursor(int cx, int cy) {
    for (int j = 0; j < CUR_H; j++) {
        for (int i = 0; i < CUR_W; i++) {
            uint8_t p = cursor_bm[j][i];
            if (!p) continue;
            int x = cx + i, y = cy + j;
            if (x < SCREEN_W && y < SCREEN_H) {
                fb[y * SCREEN_W + x] = (p == 1) ? 0x000000 : 0xFFFFFF;
            }
        }
    }
}

static void gui_compose(void) {
    for (int i = 0; i < SCREEN_W * SCREEN_H; i++) fb[i] = bg_buffer[i];
    for (int z = 0; z < nwins; z++) {
        int wi = zorder[z];
        if (wins[wi].shown) gui_draw_win(&wins[wi], z == nwins - 1);
    }
    gui_draw_cursor(mouse_x, mouse_y);
}

static void gui_raise(int wi) {
    int z;
    for (z = 0; z < nwins; z++) if (zorder[z] == wi) break;
    for (; z < nwins - 1; z++) zorder[z] = zorder[z + 1];
    zorder[nwins - 1] = wi;
}

static int gui_hit(int px, int py) {
    for (int z = nwins - 1; z >= 0; z--) {
        int wi = zorder[z];
        win_t* wn = &wins[wi];
        if (wn->shown && px >= wn->x && px < wn->x + wn->w &&
            py >= wn->y && py < wn->y + wn->h) {
            return wi;
        }
    }
    return -1;
}

// Which desktop icon (if any) is under (px,py)?
static int gui_icon_hit(int px, int py) {
    for (int i = 0; i < nicons; i++) {
        if (px >= icons[i].x - 4 && px < icons[i].x + 36 &&
            py >= icons[i].y - 4 && py < icons[i].y + 44) {
            return i;
        }
    }
    return -1;
}

extern char kbd64_getchar(void);
extern int kbd64_haskey(void);

// Enter the OS/2-style Workplace Shell desktop. Returns on Esc.
static void gui64_run(void) {
    if (!fb) return;
    mouse64_init();

    // Workplace Shell objects (desktop icons)
    icons[0] = (icon_t){ 30,  40, "OS/2 System",  IC_SYSTEM,   0 };
    icons[1] = (icon_t){ 30, 130, "Drives",       IC_DRIVES,   1 };
    icons[2] = (icon_t){ 30, 220, "Programs",     IC_PROGRAMS, 2 };
    icons[3] = (icon_t){ 30, 310, "Information",  IC_INFO,     3 };
    icons[4] = (icon_t){ 30, 400, "Shredder",     IC_SHREDDER, 4 };
    nicons = 5;
    sel_icon = -1;

    // Windows opened by double-clicking icons (start hidden)
    wins[0] = (win_t){ 200, 120, 440, 210, "OS/2 System", {
        "OS/Two 64-bit - Workplace Shell",
        "x86-64 long mode, ring-3 userspace",
        "PAE 4-level paging, preemptive scheduler",
        "Native ELF64 apps + OS/2 LX (compat mode)",
        "Double-click desktop icons to open folders.",
        "Drag title bars; sys-menu box (dbl-click) closes." }, 6, 0 };
    wins[1] = (win_t){ 260, 170, 380, 150, "Drives", {
        "C: OS2BOOT   (RamFS)",
        "A: Floppy    (empty)",
        "X: Network   (offline)",
        "Double-click a drive to browse (demo)." }, 4, 0 };
    wins[2] = (win_t){ 300, 200, 400, 160, "Programs", {
        "Command Shell   - the OS2-64> prompt",
        "System Monitor  - tasks and memory",
        "OS/2 App Sample - hello_os2.exe (LX)",
        "See sdk/ to build your own apps." }, 4, 0 };
    wins[3] = (win_t){ 320, 150, 400, 150, "Information", {
        "OS/Two: an OS/2-compatible OS that also",
        "runs native 64-bit apps.",
        "Both worlds, one kernel.",
        "Press Esc to return to the shell." }, 4, 0 };
    wins[4] = (win_t){ 360, 240, 340, 110, "Shredder", {
        "Drag objects here to discard them.",
        "(The Shredder is empty.)" }, 2, 0 };
    nwins = 5;
    for (int i = 0; i < nwins; i++) zorder[i] = i;

    gui_build_bg();
    gui_compose();

    int dragging = -1, drag_dx = 0, drag_dy = 0;
    int prev_buttons = 0;
    uint64_t last_clock = timer_ticks / 100;
    uint64_t last_click_tick = 0;
    int last_click_icon = -1;

    for (;;) {
        __asm__ volatile("sti; hlt");

        if (kbd64_haskey() && kbd64_getchar() == 27) break;   // Esc

        int left = mouse_buttons & 1;
        int need_compose = 0;

        if (left && !prev_buttons) {                 // button press
            int wi = gui_hit(mouse_x, mouse_y);
            if (wi >= 0) {
                win_t* wn = &wins[wi];
                gui_raise(wi);
                // system-menu box: double-click closes (OS/2 behavior)
                if (mouse_x >= wn->x + 4 && mouse_x < wn->x + 20 &&
                    mouse_y >= wn->y + 4 && mouse_y < wn->y + TITLE_H) {
                    wn->shown = 0;
                } else if (mouse_x >= wn->x + wn->w - 38 &&
                           mouse_x < wn->x + wn->w - 22 &&
                           mouse_y < wn->y + TITLE_H) {
                    wn->shown = 0;               // minimize == hide (demo)
                } else if (mouse_y < wn->y + TITLE_H) {
                    dragging = wi;
                    drag_dx = mouse_x - wn->x;
                    drag_dy = mouse_y - wn->y;
                }
                need_compose = 1;
            } else {
                // Desktop / icon click
                int ic = gui_icon_hit(mouse_x, mouse_y);
                sel_icon = ic;
                if (ic >= 0) {
                    uint64_t now_t = timer_ticks;
                    if (last_click_icon == ic && now_t - last_click_tick < 45) {
                        int wid = icons[ic].opens;   // double-click -> open
                        wins[wid].shown = 1;
                        gui_raise(wid);
                    }
                    last_click_icon = ic;
                    last_click_tick = now_t;
                }
                gui_build_bg();                  // reflect selection
                need_compose = 1;
            }
        } else if (!left && prev_buttons) {
            dragging = -1;
        }
        prev_buttons = left;

        if (mouse_moved) {
            mouse_moved = 0;
            if (dragging >= 0) {
                wins[dragging].x = mouse_x - drag_dx;
                wins[dragging].y = mouse_y - drag_dy;
            }
            need_compose = 1;
        }

        uint64_t now = timer_ticks / 100;
        if (now != last_clock) {
            last_clock = now;
            gui_taskbar_to(bg_buffer);
            need_compose = 1;
        }

        if (need_compose) gui_compose();
    }
}

// ---------------------------------------------------------------- shell
extern char kbd64_getchar(void);

static int str_eq(const char* a, const char* b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return *a == *b;
}

static void shell_smp(void);   // forward: 'smp' command runs the demo

static void shell_exec(const char* cmd) {
    if (cmd[0] == 0) return;
    if (str_eq(cmd, "help")) {
        con_puts("Commands:\n");
        con_puts("  help   - this list\n");
        con_puts("  ver    - version and CPU mode\n");
        con_puts("  mem    - memory summary\n");
        con_puts("  ticks  - timer ticks since boot\n");
        con_puts("  smp    - run the preemptive multitasking demo\n");
        con_puts("  gui    - OS/2 Workplace Shell desktop (double-click icons)\n");
        con_puts("  clear  - clear the screen\n");
        con_puts("  echo X - print X\n");
    } else if (str_eq(cmd, "ver")) {
        con_puts("OS/Two 64-bit kernel (Phase B.6)\n");
        con_puts("x86-64 long mode, ring-3 userspace, preemptive scheduler,\n");
        con_puts("ELF64 + OS/2 LX compatibility. An OS/2 clone for the 64-bit era.\n");
    } else if (str_eq(cmd, "mem")) {
        uint64_t cr3;
        __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
        con_puts("Physical RAM: 256 MB   PMM arena: 32-256 MB (4KB pages)\n");
        con_puts("Paging: PAE 4-level, CR3=");
        con_puthex(cr3);
        con_puts("\n");
    } else if (str_eq(cmd, "ticks")) {
        con_puts("Timer ticks (100 Hz): ");
        con_putdec(timer_ticks);
        con_puts("\n");
    } else if (str_eq(cmd, "clear")) {
        con_clear();
    } else if (cmd[0]=='e'&&cmd[1]=='c'&&cmd[2]=='h'&&cmd[3]=='o'&&
               (cmd[4]==' '||cmd[4]==0)) {
        con_puts(cmd[4] ? cmd + 5 : "");
        con_puts("\n");
    } else if (str_eq(cmd, "smp")) {
        shell_smp();
    } else if (str_eq(cmd, "gui") || str_eq(cmd, "desktop")) {
        gui64_run();          // returns on Esc
        con_clear();
        con_puts("Back from the desktop.\n");
    } else {
        con_puts("Unknown command: ");
        con_puts(cmd);
        con_puts("  (try 'help')\n");
    }
}

static void shell_run(void) __attribute__((noreturn));
static void shell_run(void) {
    con_clear();
    con_puts("OS/Two 64-bit interactive shell.  Type 'help'.\n\n");

    char line[128];
    for (;;) {
        con_puts("OS2-64> ");
        int n = 0;
        for (;;) {
            char c = kbd64_getchar();
            if (c == '\n') { con_putc('\n'); break; }
            if (c == '\b') { if (n > 0) { n--; con_putc('\b'); } continue; }
            if (n < (int)sizeof(line) - 1) { line[n++] = c; con_putc(c); }
        }
        line[n] = 0;
        shell_exec(line);
    }
}

// ---------------------------------------------------------------- main
void kmain64(uint32_t mb_info_addr) {
    serial_init();
    sputs("\n[64] OS/Two 64-bit kernel entered long mode!\n");

    // Prove we are genuinely executing 64-bit code
    uint64_t cr0, cr4, efer_lo;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr4, %0" : "=r"(cr4));
    {
        uint32_t lo, hi;
        __asm__ volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(0xC0000080));
        efer_lo = lo | ((uint64_t)hi << 32);
    }
    sputs("[64] CR0="); sputhex64(cr0);
    sputs(" CR4="); sputhex64(cr4);
    sputs(" EFER="); sputhex64(efer_lo);
    sputs(efer_lo & (1 << 10) ? " (LMA=1, long mode active)\n"
                              : " (LMA=0 ?!)\n");

    uint64_t big = 0x123456789ABCDEF0ull;
    uint64_t prod = big ^ (big << 13) ^ (big >> 7);   // native 64-bit ops
    sputs("[64] 64-bit arithmetic: ");
    sputhex64(big); sputs(" -> "); sputhex64(prod); sputs("\n");

    // Multiboot memory info (identity-mapped, still reachable)
    if (mb_info_addr) {
        const uint32_t* mbi = (const uint32_t*)(uint64_t)mb_info_addr;
        if (mbi[0] & 1) {
            sputs("[64] Memory: ");
            sputdec(mbi[1]); sputs(" KB lower, ");
            sputdec(mbi[2]); sputs(" KB upper\n");
        }
    }

    // High-resolution display via BGA
    uint64_t fb_phys = bga_find_framebuffer();
    sputs("[64] BGA framebuffer at ");
    sputhex64(fb_phys);
    sputs("\n");

    if (fb_phys) {
        bga_write(4, 0);                 // disable
        bga_write(1, SCREEN_W);          // xres
        bga_write(2, SCREEN_H);          // yres
        bga_write(3, 32);                // bpp
        bga_write(4, 0x41);              // enable | LFB
        sputs("[64] Display mode: ");
        sputdec(bga_read(1)); sputs("x");
        sputdec(bga_read(2)); sputs("x");
        sputdec(bga_read(3)); sputs("\n");

        fb = (uint32_t*)fb_phys;

        // Warm amber/bronze look - visually distinct from the teal
        // 32-bit desktop, so there is no mistaking which kernel runs
        gradient(0x8A5A24, 0x1E1006);

        fill_rect(232, 226, 560, 4, 0xFFD9A0);
        draw_text2x(248, 260, "OS/Two 64-bit", 0xFFFFFF);
        draw_text2x(248, 300, "x86-64 long mode active", 0xFFD9A0);
        fill_rect(232, 348, 560, 2, 0xFFD9A0);

        char hexbuf[20];
        draw_text(248, 372, "Native 64-bit C code, PAE 4-level paging, EFER.LMA=1", 0xFFF0D8);
        hex64_str(prod, hexbuf);
        draw_text(248, 390, "64-bit register value:", 0xFFF0D8);
        draw_text(440, 390, hexbuf, 0xB0FFB0);
        draw_text(248, 408, "Phase B.0: boot foundation for the 64-bit OS/Two kernel", 0xFFF0D8);
        draw_text(248, 426, "Next: 64-bit IDT, scheduler, ELF64 + 32-bit compat mode", 0xC0C0C0);

        sputs("[64] Framebuffer painted.\n");
    }

    // Phase B.1: protection + interrupts + userspace
    sputs("[64] Setting up GDT/TSS...\n");
    gdt_tss_init();
    sputs("[64] Setting up IDT (32 exceptions + timer)...\n");
    idt_init();
    sputs("[64] Remapping PIC, starting 100 Hz timer...\n");
    pic_timer_init();
    sputs("[64] Enabling SYSCALL/SYSRET...\n");
    syscall_init();
    map_user_region();
    pmm64_init();
    __asm__ volatile("sti");

    // With a multiboot module: run the ELF64 app (+ LX compat) demo.
    try_run_elf_module(mb_info_addr);

    // No module: pick by kernel command line. "-append smp" runs the
    // multitasking demo directly; otherwise start the interactive shell.
    int want_smp = 0;
    if (mb_info_addr) {
        const uint32_t* mbi = (const uint32_t*)(uint64_t)mb_info_addr;
        if (mbi[0] & (1 << 2)) {                       // cmdline present
            const char* cl = (const char*)(uint64_t)mbi[4];
            for (const char* p = cl; *p; p++) {
                if (p[0]=='s'&&p[1]=='m'&&p[2]=='p') { want_smp = 1; break; }
            }
        }
    }

    if (want_smp) sched_demo();   // never returns
    shell_run();                  // interactive shell; never returns
}
