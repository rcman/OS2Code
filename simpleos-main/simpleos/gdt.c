// File: gdt.c
// Global Descriptor Table setup

#include "types.h"

// GDT entry structure
struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_middle;
    uint8_t access;
    uint8_t granularity;
    uint8_t base_high;
} __attribute__((packed));

// GDT pointer structure
struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

// TSS structure
struct tss_entry {
    uint32_t prev_tss;
    uint32_t esp0;          // Stack pointer for ring 0
    uint32_t ss0;           // Stack segment for ring 0
    uint32_t esp1;
    uint32_t ss1;
    uint32_t esp2;
    uint32_t ss2;
    uint32_t cr3;
    uint32_t eip;
    uint32_t eflags;
    uint32_t eax;
    uint32_t ecx;
    uint32_t edx;
    uint32_t ebx;
    uint32_t esp;
    uint32_t ebp;
    uint32_t esi;
    uint32_t edi;
    uint32_t es;
    uint32_t cs;
    uint32_t ss;
    uint32_t ds;
    uint32_t fs;
    uint32_t gs;
    uint32_t ldt;
    uint16_t trap;
    uint16_t iomap_base;
} __attribute__((packed));

// GDT entries (6 entries: null, code, data, user code, user data, TSS)
static struct gdt_entry gdt[6];
static struct gdt_ptr gdt_pointer;
static struct tss_entry tss;

// External assembly functions
extern void gdt_flush(uint32_t);
extern void tss_flush(void);

// Set a GDT entry
static void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {
    gdt[num].base_low = (base & 0xFFFF);
    gdt[num].base_middle = (base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    
    gdt[num].limit_low = (limit & 0xFFFF);
    gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    
    gdt[num].access = access;
}

// Initialize TSS
static void tss_init(uint32_t ss0, uint32_t esp0) {
    uint32_t base = (uint32_t)&tss;
    uint32_t limit = sizeof(tss);
    
    // Add TSS descriptor to GDT
    gdt_set_gate(5, base, limit, 0xE9, 0x00);
    
    // Zero out TSS
    uint8_t* tss_ptr = (uint8_t*)&tss;
    for (uint32_t i = 0; i < sizeof(tss); i++) {
        tss_ptr[i] = 0;
    }
    
    // Set stack for ring 0
    tss.ss0 = ss0;
    tss.esp0 = esp0;
    
    // Set segments
    tss.cs = 0x0b;  // Kernel code | RPL 3
    tss.ss = tss.ds = tss.es = tss.fs = tss.gs = 0x13;  // Kernel data | RPL 3
    
    tss.iomap_base = sizeof(tss);
}

// Set kernel stack in TSS (for task switching)
void tss_set_kernel_stack(uint32_t stack) {
    tss.esp0 = stack;
}

// Initialize GDT
void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * 6) - 1;
    gdt_pointer.base = (uint32_t)&gdt;
    
    // Null segment
    gdt_set_gate(0, 0, 0, 0, 0);
    
    // Kernel code segment: base=0, limit=4GB, ring 0
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    // Kernel data segment: base=0, limit=4GB, ring 0
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    // User code segment: base=0, limit=4GB, ring 3
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    // User data segment: base=0, limit=4GB, ring 3
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);
    
    // TSS (kernel stack at 0x90000 for now)
    tss_init(0x10, 0x90000);
    
    // Load GDT
    gdt_flush((uint32_t)&gdt_pointer);
    
    // Load TSS
    tss_flush();
}
