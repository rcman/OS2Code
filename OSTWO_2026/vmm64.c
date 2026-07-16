// File: vmm64.c
// 4KB-granular virtual memory mapping for the 64-bit kernel.
//
// The boot trampoline identity-maps 0..4GB with 2MB pages; this module
// adds 4KB user mappings ABOVE 4GB (where the page-table walk is still
// empty), so ELF64 applications link at 0x100000000+ without colliding
// with the identity map. Intermediate tables come from pmm64.

#include <stdint.h>

extern uint64_t pmm64_alloc_page(void);

#define PTE_P   0x001ull
#define PTE_W   0x002ull
#define PTE_U   0x004ull

// Map one 4KB page. Intermediate entries get user+writable (the leaf
// controls the effective permissions). Returns 0 on success.
int vmm64_map_page(uint64_t vaddr, uint64_t phys, int user, int writable) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    uint64_t* table = (uint64_t*)(cr3 & ~0xFFFull);

    // Walk PML4 -> PDPT -> PD, creating tables as needed
    for (int level = 3; level >= 1; level--) {
        uint64_t idx = (vaddr >> (12 + 9 * level)) & 0x1FF;
        if (!(table[idx] & PTE_P)) {
            uint64_t nt = pmm64_alloc_page();
            if (nt == 0) return -1;
            table[idx] = nt | PTE_P | PTE_W | PTE_U;
        } else if (table[idx] & 0x80) {
            return -2;      // huge page already covers this range
        } else if (user) {
            table[idx] |= PTE_U;
        }
        table = (uint64_t*)(table[idx] & ~0xFFFull & 0xFFFFFFFFFFFFull);
    }

    uint64_t pt_idx = (vaddr >> 12) & 0x1FF;
    table[pt_idx] = (phys & ~0xFFFull) | PTE_P |
                    (writable ? PTE_W : 0) | (user ? PTE_U : 0);
    __asm__ volatile("invlpg (%0)" : : "r"(vaddr) : "memory");
    return 0;
}
