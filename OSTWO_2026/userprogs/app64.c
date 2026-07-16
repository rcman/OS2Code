/* app64.c - a native 64-bit OS/Two application.
 *
 * Compiled by the host GCC as a static ELF64 binary linked at 4GB
 * (above the kernel's identity map) and loaded by the OSTwo 64-bit
 * kernel's ELF64 loader from a multiboot module.
 *
 * Syscall ABI: RAX = number, RDI/RSI/RDX = args, result in RAX.
 *   1 = print string   2 = print hex value   3 = exit   4 = get ticks
 */

typedef unsigned long long u64;

static inline u64 sys(u64 nr, u64 a1) {
    u64 ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(nr), "D"(a1)
                     : "rcx", "r11", "memory");
    return ret;
}

static void print(const char* s) { sys(1, (u64)s); }
static void print_hex(u64 v)     { sys(2, v); }

/* 64-bit Fibonacci - F(90) = 2880067194370816120 needs 62 bits */
static u64 fib(int n) {
    u64 a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        u64 t = a + b;
        a = b;
        b = t;
    }
    return a;
}

/* xorshift64 PRNG - classic 64-bit bit-twiddling */
static u64 xorshift64(u64 x) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

void _start(void) {
    print("Hello! I am app64.elf: a real ELF64 binary, compiled by GCC,\n");
    print("loaded from disk by the OS/Two 64-bit kernel's ELF64 loader.\n");

    print("F(90), a 62-bit Fibonacci number:\n");
    print_hex(fib(90));

    print("xorshift64 chain from the Fibonacci value:\n");
    u64 x = fib(90);
    for (int i = 0; i < 3; i++) {
        x = xorshift64(x);
        print_hex(x);
    }

    u64 t0 = sys(4, 0);
    /* burn some cycles so ticks advance */
    volatile u64 spin = 0;
    for (u64 i = 0; i < 30000000ull; i++) spin += i;
    u64 t1 = sys(4, 0);
    print("Timer ticks elapsed while spinning (100 Hz):\n");
    print_hex(t1 - t0);

    print("app64 done - exiting via syscall.\n");
    sys(3, 0);
    for (;;) { }
}
