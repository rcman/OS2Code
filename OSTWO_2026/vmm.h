// File: vmm.h
// Virtual Memory Manager - Paging support

#ifndef VMM_H
#define VMM_H

#include "types.h"

// Page table/directory entry flags
#define PTE_PRESENT     (1 << 0)    // Page is present in memory
#define PTE_WRITABLE    (1 << 1)    // Page is writable
#define PTE_USER        (1 << 2)    // Page is accessible from user mode
#define PTE_WRITETHROUGH (1 << 3)   // Write-through caching
#define PTE_NOCACHE     (1 << 4)    // Disable caching
#define PTE_ACCESSED    (1 << 5)    // Page has been accessed
#define PTE_DIRTY       (1 << 6)    // Page has been written to
#define PTE_HUGE        (1 << 7)    // 4MB page (in PDE only)
#define PTE_GLOBAL      (1 << 8)    // Global page (not flushed on CR3 change)

// Address masks
#define PTE_FRAME_MASK  0xFFFFF000  // Physical frame address
#define PTE_FLAGS_MASK  0x00000FFF  // Flag bits

// Page sizes
#define PAGE_SIZE       4096
#define PAGE_ENTRIES    1024        // Entries per page table/directory

// Virtual address breakdown
#define VADDR_PD_INDEX(addr)  (((addr) >> 22) & 0x3FF)  // Page directory index
#define VADDR_PT_INDEX(addr)  (((addr) >> 12) & 0x3FF)  // Page table index
#define VADDR_OFFSET(addr)    ((addr) & 0xFFF)          // Offset within page

// Kernel virtual address space starts at 3GB
#define KERNEL_VBASE    0xC0000000
#define KERNEL_PBASE    0x00100000  // Kernel loaded at 1MB physical

// Page directory entry
typedef uint32_t pde_t;

// Page table entry  
typedef uint32_t pte_t;

// Page directory (array of 1024 PDEs)
typedef pde_t page_directory_t[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

// Page table (array of 1024 PTEs)
typedef pte_t page_table_t[PAGE_ENTRIES] __attribute__((aligned(PAGE_SIZE)));

// Initialize virtual memory manager
void vmm_init(void);

// Create a new page directory (for a new process)
uint32_t vmm_create_page_directory(void);

// Destroy a page directory
void vmm_destroy_page_directory(uint32_t pd_phys);

// Switch to a page directory
void vmm_switch_page_directory(uint32_t pd_phys);

// Get current page directory physical address
uint32_t vmm_get_current_directory(void);

// Map a virtual address to a physical address
bool vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

// Unmap a virtual address
void vmm_unmap_page(uint32_t virt);

// Map a range of pages
bool vmm_map_range(uint32_t virt_start, uint32_t phys_start, uint32_t size, uint32_t flags);

// Unmap a range of pages
void vmm_unmap_range(uint32_t virt_start, uint32_t size);

// Get physical address for a virtual address (returns 0 if not mapped)
uint32_t vmm_get_physical(uint32_t virt);

// Check if a virtual address is mapped
bool vmm_is_mapped(uint32_t virt);

// Allocate and map a page at virtual address
bool vmm_alloc_page(uint32_t virt, uint32_t flags);

// Free and unmap a page
void vmm_free_page(uint32_t virt);

// Page fault handler
void vmm_page_fault_handler(uint32_t fault_addr, uint32_t error_code);

// Invalidate TLB entry for address
void vmm_invalidate_page(uint32_t virt);

// Flush entire TLB
void vmm_flush_tlb(void);

// Debug: print page directory info
void vmm_print_mappings(void);

#endif // VMM_H
