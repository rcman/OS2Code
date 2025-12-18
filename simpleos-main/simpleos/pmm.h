// File: pmm.h
// Physical Memory Manager - Bitmap-based page allocator

#ifndef PMM_H
#define PMM_H

#include "types.h"

// Page size (4KB)
#define PAGE_SIZE 4096
#define PAGE_SHIFT 12

// Align address down to page boundary
#define PAGE_ALIGN_DOWN(addr) ((addr) & ~(PAGE_SIZE - 1))

// Align address up to page boundary
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

// Convert address to page frame number
#define ADDR_TO_PFN(addr) ((addr) >> PAGE_SHIFT)

// Convert page frame number to address
#define PFN_TO_ADDR(pfn) ((pfn) << PAGE_SHIFT)

// Memory region types (from multiboot)
#define MMAP_TYPE_AVAILABLE 1
#define MMAP_TYPE_RESERVED  2
#define MMAP_TYPE_ACPI      3
#define MMAP_TYPE_NVS       4
#define MMAP_TYPE_BADRAM    5

// Multiboot memory map entry
typedef struct {
    uint32_t size;
    uint64_t base_addr;
    uint64_t length;
    uint32_t type;
} __attribute__((packed)) mmap_entry_t;

// Initialize PMM with memory map from multiboot
void pmm_init(uint32_t mmap_addr, uint32_t mmap_length, uint32_t kernel_end);

// Allocate a single physical page (returns physical address, 0 on failure)
uint32_t pmm_alloc_page(void);

// Allocate contiguous physical pages (returns physical address, 0 on failure)
uint32_t pmm_alloc_pages(uint32_t count);

// Free a single physical page
void pmm_free_page(uint32_t phys_addr);

// Free contiguous physical pages
void pmm_free_pages(uint32_t phys_addr, uint32_t count);

// Get total physical memory in bytes
uint32_t pmm_get_total_memory(void);

// Get free physical memory in bytes
uint32_t pmm_get_free_memory(void);

// Get used physical memory in bytes
uint32_t pmm_get_used_memory(void);

// Mark a region as used (for kernel, reserved areas)
void pmm_mark_region_used(uint32_t base, uint32_t length);

// Mark a region as free
void pmm_mark_region_free(uint32_t base, uint32_t length);

// Debug: print memory statistics
void pmm_print_stats(void);

#endif // PMM_H
