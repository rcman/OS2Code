// File: pmm64.c
// Physical memory manager for the 64-bit kernel (Phase B.2).
//
// Bitmap allocator managing the region 32MB..256MB. Everything below
// 32MB is treated as reserved (BIOS area, loader at 1MB, kernel
// payload at 2MB, user blob at 16MB, multiboot modules). QEMU runs
// with -m 256M; multiboot memory-map integration comes with B.3.

#include <stdint.h>

#define PMM64_BASE   0x02000000ull   // 32MB
#define PMM64_LIMIT  0x10000000ull   // 256MB
#define PMM64_PAGES  ((PMM64_LIMIT - PMM64_BASE) / 4096)

static uint64_t bitmap[PMM64_PAGES / 64];

void pmm64_init(void) {
    for (uint64_t i = 0; i < PMM64_PAGES / 64; i++) {
        bitmap[i] = 0;
    }
}

// Allocate one zeroed 4KB page. Returns physical address (identity
// mapped, so directly usable as a pointer) or 0 when exhausted.
uint64_t pmm64_alloc_page(void) {
    for (uint64_t i = 0; i < PMM64_PAGES / 64; i++) {
        if (bitmap[i] != ~0ull) {
            for (int b = 0; b < 64; b++) {
                if (!(bitmap[i] & (1ull << b))) {
                    bitmap[i] |= (1ull << b);
                    uint64_t phys = PMM64_BASE + ((i * 64 + b) * 4096);
                    uint64_t* p = (uint64_t*)phys;
                    for (int j = 0; j < 512; j++) p[j] = 0;
                    return phys;
                }
            }
        }
    }
    return 0;
}

void pmm64_free_page(uint64_t phys) {
    if (phys < PMM64_BASE || phys >= PMM64_LIMIT) return;
    uint64_t idx = (phys - PMM64_BASE) / 4096;
    bitmap[idx / 64] &= ~(1ull << (idx % 64));
}
