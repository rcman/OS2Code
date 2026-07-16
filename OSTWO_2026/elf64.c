// File: elf64.c
// ELF64 loader for the 64-bit kernel (Phase B.2).
//
// Loads PT_LOAD segments of a static ELF64 executable into fresh
// physical pages mapped at the linked virtual addresses (expected to
// be >= 4GB; see vmm64.c). Returns the entry point.

#include <stdint.h>

extern uint64_t pmm64_alloc_page(void);
extern int vmm64_map_page(uint64_t vaddr, uint64_t phys, int user, int writable);
extern void sputs(const char* s);
extern void sputhex64(uint64_t v);

typedef struct {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed)) Elf64_Ehdr;

typedef struct {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t filesz;
    uint64_t memsz;
    uint64_t align;
} __attribute__((packed)) Elf64_Phdr;

#define PT_LOAD 1
#define PF_W    2

// Load an ELF64 image; returns entry point or 0 on error.
uint64_t elf64_load(const uint8_t* data, uint64_t size) {
    if (size < sizeof(Elf64_Ehdr)) return 0;
    const Elf64_Ehdr* eh = (const Elf64_Ehdr*)data;

    if (eh->ident[0] != 0x7F || eh->ident[1] != 'E' ||
        eh->ident[2] != 'L' || eh->ident[3] != 'F') {
        sputs("[64] ELF64: bad magic\n");
        return 0;
    }
    if (eh->ident[4] != 2) {            // ELFCLASS64
        sputs("[64] ELF64: not a 64-bit ELF\n");
        return 0;
    }
    if (eh->machine != 62) {            // EM_X86_64
        sputs("[64] ELF64: not x86-64\n");
        return 0;
    }

    const Elf64_Phdr* ph = (const Elf64_Phdr*)(data + eh->phoff);
    for (int i = 0; i < eh->phnum; i++) {
        if (ph[i].type != PT_LOAD) continue;

        uint64_t va = ph[i].vaddr & ~0xFFFull;
        uint64_t end = (ph[i].vaddr + ph[i].memsz + 0xFFF) & ~0xFFFull;
        sputs("[64] ELF64 segment: vaddr=");
        sputhex64(ph[i].vaddr);
        sputs(" filesz=");
        sputhex64(ph[i].filesz);
        sputs("\n");

        for (uint64_t page = va; page < end; page += 4096) {
            uint64_t phys = pmm64_alloc_page();
            if (phys == 0) return 0;
            if (vmm64_map_page(page, phys, 1, (ph[i].flags & PF_W) ? 1 : 0)) {
                sputs("[64] ELF64: map failed\n");
                return 0;
            }
            // Copy this page's slice of file data (page is zeroed by pmm)
            uint64_t file_lo = ph[i].vaddr + 0;      // segment file window
            uint64_t file_hi = ph[i].vaddr + ph[i].filesz;
            uint64_t lo = page > file_lo ? page : file_lo;
            uint64_t hi = (page + 4096) < file_hi ? (page + 4096) : file_hi;
            if (lo < hi) {
                const uint8_t* src = data + ph[i].offset + (lo - ph[i].vaddr);
                uint8_t* dst = (uint8_t*)(phys + (lo - page));
                for (uint64_t j = 0; j < hi - lo; j++) dst[j] = src[j];
            }
        }
    }

    return eh->entry;
}
