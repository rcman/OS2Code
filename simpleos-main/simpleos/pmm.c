// File: pmm.c
// Physical Memory Manager - Bitmap-based page allocator

#include "pmm.h"

// External functions
extern void printf(const char* format, ...);

// Maximum physical memory we support (256 MB for now)
#define MAX_PHYSICAL_MEMORY (256 * 1024 * 1024)
#define MAX_PAGES (MAX_PHYSICAL_MEMORY / PAGE_SIZE)
#define BITMAP_SIZE (MAX_PAGES / 32)

// Bitmap: 1 = used, 0 = free
static uint32_t page_bitmap[BITMAP_SIZE];

// Memory statistics
static uint32_t total_pages = 0;
static uint32_t used_pages = 0;
static uint32_t total_memory = 0;

// Set a bit in the bitmap (mark page as used)
static inline void bitmap_set(uint32_t pfn) {
    if (pfn < MAX_PAGES) {
        page_bitmap[pfn / 32] |= (1 << (pfn % 32));
    }
}

// Clear a bit in the bitmap (mark page as free)
static inline void bitmap_clear(uint32_t pfn) {
    if (pfn < MAX_PAGES) {
        page_bitmap[pfn / 32] &= ~(1 << (pfn % 32));
    }
}

// Test a bit in the bitmap
static inline bool bitmap_test(uint32_t pfn) {
    if (pfn < MAX_PAGES) {
        return (page_bitmap[pfn / 32] & (1 << (pfn % 32))) != 0;
    }
    return true; // Out of range = used
}

// Find first free page starting from given pfn
static uint32_t find_free_page(uint32_t start_pfn) {
    for (uint32_t i = start_pfn; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            return i;
        }
    }
    return 0xFFFFFFFF; // Not found
}

// Find first sequence of free pages
static uint32_t find_free_pages(uint32_t count, uint32_t start_pfn) {
    uint32_t consecutive = 0;
    uint32_t start = 0;
    
    for (uint32_t i = start_pfn; i < total_pages; i++) {
        if (!bitmap_test(i)) {
            if (consecutive == 0) {
                start = i;
            }
            consecutive++;
            if (consecutive == count) {
                return start;
            }
        } else {
            consecutive = 0;
        }
    }
    return 0xFFFFFFFF; // Not found
}

// Initialize PMM with memory map from multiboot
void pmm_init(uint32_t mmap_addr, uint32_t mmap_length, uint32_t kernel_end) {
    // First, mark ALL pages as used
    for (uint32_t i = 0; i < BITMAP_SIZE; i++) {
        page_bitmap[i] = 0xFFFFFFFF;
    }
    
    // Reset counters  
    total_memory = 0;
    total_pages = 0;
    used_pages = 0;
    
    // Parse multiboot memory map and mark available regions as free
    mmap_entry_t* mmap = (mmap_entry_t*)mmap_addr;
    mmap_entry_t* mmap_end = (mmap_entry_t*)(mmap_addr + mmap_length);
    
    printf("[PMM] Parsing memory map at 0x%x, length %d\n", mmap_addr, mmap_length);
    
    while (mmap < mmap_end) {
        uint32_t base = (uint32_t)mmap->base_addr;
        uint32_t length = (uint32_t)mmap->length;
        uint32_t type = mmap->type;
        
        printf("[PMM]   Region: base=0x%x, len=0x%x, type=%d", base, length, type);
        
        if (type == MMAP_TYPE_AVAILABLE) {
            printf(" (avail)\n");
            
            // Track total available memory
            total_memory += length;
            
            // Only use memory below our maximum
            if (base < MAX_PHYSICAL_MEMORY) {
                uint32_t end = base + length;
                if (end > MAX_PHYSICAL_MEMORY) {
                    end = MAX_PHYSICAL_MEMORY;
                }
                
                // Mark this region as free directly  
                uint32_t start_pfn = ADDR_TO_PFN(PAGE_ALIGN_UP(base));
                uint32_t end_pfn = ADDR_TO_PFN(PAGE_ALIGN_DOWN(end));
                
                for (uint32_t pfn = start_pfn; pfn < end_pfn && pfn < MAX_PAGES; pfn++) {
                    bitmap_clear(pfn);
                }
            }
        } else {
            printf(" (rsvd)\n");
        }
        
        // Move to next entry (size field doesn't include itself)
        mmap = (mmap_entry_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
    
    // Calculate total pages based on actual available memory
    total_pages = total_memory / PAGE_SIZE;
    if (total_pages > MAX_PAGES) {
        total_pages = MAX_PAGES;
    }
    
    // Reserve first 1MB (BIOS, legacy stuff)
    for (uint32_t pfn = 0; pfn < ADDR_TO_PFN(0x100000); pfn++) {
        bitmap_set(pfn);
    }
    
    // Reserve kernel (1MB to kernel_end)
    uint32_t kernel_start = 0x100000;
    uint32_t kernel_end_aligned = PAGE_ALIGN_UP(kernel_end);
    uint32_t kernel_pages = (kernel_end_aligned - kernel_start) / PAGE_SIZE;
    
    for (uint32_t i = 0; i < kernel_pages; i++) {
        bitmap_set(ADDR_TO_PFN(kernel_start) + i);
    }
    
    printf("[PMM] Reserved kernel: 0x%x - 0x%x (%d KB)\n", 
           kernel_start, kernel_end_aligned, (kernel_end_aligned - kernel_start) / 1024);
    
    // Now count used pages accurately
    used_pages = 0;
    for (uint32_t pfn = 0; pfn < total_pages; pfn++) {
        if (bitmap_test(pfn)) {
            used_pages++;
        }
    }
    
    pmm_print_stats();
}

// Allocate a single physical page
uint32_t pmm_alloc_page(void) {
    // Start searching from 1MB (skip low memory)
    uint32_t pfn = find_free_page(ADDR_TO_PFN(0x100000));
    
    if (pfn == 0xFFFFFFFF) {
        printf("[PMM] ERROR: Out of physical memory!\n");
        return 0;
    }
    
    bitmap_set(pfn);
    used_pages++;
    
    return PFN_TO_ADDR(pfn);
}

// Allocate contiguous physical pages
uint32_t pmm_alloc_pages(uint32_t count) {
    if (count == 0) return 0;
    if (count == 1) return pmm_alloc_page();
    
    uint32_t pfn = find_free_pages(count, ADDR_TO_PFN(0x100000));
    
    if (pfn == 0xFFFFFFFF) {
        printf("[PMM] ERROR: Cannot allocate %d contiguous pages!\n", count);
        return 0;
    }
    
    for (uint32_t i = 0; i < count; i++) {
        bitmap_set(pfn + i);
    }
    used_pages += count;
    
    return PFN_TO_ADDR(pfn);
}

// Free a single physical page
void pmm_free_page(uint32_t phys_addr) {
    uint32_t pfn = ADDR_TO_PFN(phys_addr);
    
    if (pfn < total_pages && bitmap_test(pfn)) {
        bitmap_clear(pfn);
        if (used_pages > 0) used_pages--;
    }
}

// Free contiguous physical pages
void pmm_free_pages(uint32_t phys_addr, uint32_t count) {
    uint32_t pfn = ADDR_TO_PFN(phys_addr);
    
    for (uint32_t i = 0; i < count; i++) {
        if (pfn + i < total_pages && bitmap_test(pfn + i)) {
            bitmap_clear(pfn + i);
            if (used_pages > 0) used_pages--;
        }
    }
}

// Mark a region as used
void pmm_mark_region_used(uint32_t base, uint32_t length) {
    uint32_t start_pfn = ADDR_TO_PFN(PAGE_ALIGN_DOWN(base));
    uint32_t end_pfn = ADDR_TO_PFN(PAGE_ALIGN_UP(base + length));
    
    for (uint32_t pfn = start_pfn; pfn < end_pfn && pfn < MAX_PAGES; pfn++) {
        if (!bitmap_test(pfn)) {
            bitmap_set(pfn);
            used_pages++;
        }
    }
}

// Mark a region as free
void pmm_mark_region_free(uint32_t base, uint32_t length) {
    uint32_t start_pfn = ADDR_TO_PFN(PAGE_ALIGN_UP(base));
    uint32_t end_pfn = ADDR_TO_PFN(PAGE_ALIGN_DOWN(base + length));
    
    for (uint32_t pfn = start_pfn; pfn < end_pfn && pfn < MAX_PAGES; pfn++) {
        if (bitmap_test(pfn)) {
            bitmap_clear(pfn);
            if (used_pages > 0) used_pages--;
        }
    }
}

// Get total physical memory
uint32_t pmm_get_total_memory(void) {
    return total_pages * PAGE_SIZE;
}

// Get free physical memory
uint32_t pmm_get_free_memory(void) {
    return (total_pages - used_pages) * PAGE_SIZE;
}

// Get used physical memory
uint32_t pmm_get_used_memory(void) {
    return used_pages * PAGE_SIZE;
}

// Print memory statistics
void pmm_print_stats(void) {
    uint32_t total_kb = pmm_get_total_memory() / 1024;
    uint32_t used_kb = pmm_get_used_memory() / 1024;
    uint32_t free_kb = pmm_get_free_memory() / 1024;
    
    printf("[PMM] Memory: %d KB total, %d KB used, %d KB free\n",
           total_kb, used_kb, free_kb);
    printf("[PMM] Pages: %d total, %d used, %d free\n",
           total_pages, used_pages, total_pages - used_pages);
}
