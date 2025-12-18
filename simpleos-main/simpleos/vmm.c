// File: vmm.c
// Virtual Memory Manager - Paging support

#include "vmm.h"
#include "pmm.h"

// External functions
extern void printf(const char* format, ...);

// Kernel page directory (identity mapped initially)
static page_directory_t kernel_page_directory __attribute__((aligned(PAGE_SIZE)));

// First page table for identity mapping first 4MB
static page_table_t first_page_table __attribute__((aligned(PAGE_SIZE)));

// Current page directory physical address
static uint32_t current_pd_phys = 0;

// Assembly helpers
static inline void load_page_directory(uint32_t pd_phys) {
    __asm__ volatile("mov %0, %%cr3" : : "r"(pd_phys) : "memory");
}

static inline void enable_paging(void) {
    uint32_t cr0;
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;  // Set PG bit
    __asm__ volatile("mov %0, %%cr0" : : "r"(cr0) : "memory");
}

static inline uint32_t get_cr2(void) {
    uint32_t cr2;
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    return cr2;
}

static inline void invlpg(uint32_t virt) {
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// Zero a page
static void zero_page(uint32_t phys_addr) {
    uint32_t* page = (uint32_t*)phys_addr;
    for (int i = 0; i < PAGE_SIZE / 4; i++) {
        page[i] = 0;
    }
}

// Get page table for a virtual address, optionally creating it
static pte_t* get_page_table(uint32_t virt, bool create) {
    uint32_t pd_index = VADDR_PD_INDEX(virt);
    // Use current page directory (not always kernel_page_directory)
    pde_t* pd = (pde_t*)current_pd_phys;
    
    if (!(pd[pd_index] & PTE_PRESENT)) {
        if (!create) {
            return NULL;
        }
        
        // Allocate a new page table
        uint32_t pt_phys = pmm_alloc_page();
        if (pt_phys == 0) {
            printf("[VMM] ERROR: Cannot allocate page table!\n");
            return NULL;
        }
        
        // Zero the page table
        zero_page(pt_phys);
        
        // Set up page directory entry
        pd[pd_index] = pt_phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }
    
    // Return pointer to page table
    return (pte_t*)(pd[pd_index] & PTE_FRAME_MASK);
}

// Initialize VMM
void vmm_init(void) {
    printf("[VMM] Initializing virtual memory...\n");
    
    // Clear page directory
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        kernel_page_directory[i] = 0;
    }
    
    // Clear first page table
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        first_page_table[i] = 0;
    }
    
    // Identity map first 4MB (for kernel and low memory)
    // This maps virtual 0x00000000-0x003FFFFF to physical 0x00000000-0x003FFFFF
    // Mark as user-accessible so user mode code can execute here
    for (uint32_t i = 0; i < PAGE_ENTRIES; i++) {
        uint32_t phys = i * PAGE_SIZE;
        first_page_table[i] = phys | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    }

    // Set up page directory entry for first 4MB
    kernel_page_directory[0] = (uint32_t)first_page_table | PTE_PRESENT | PTE_WRITABLE | PTE_USER;
    
    // Map kernel at higher half (0xC0000000+) - same physical memory
    // This allows us to later unmap the lower mapping
    // For now, we'll keep identity mapping and add higher-half later
    
    // Get physical address of page directory
    current_pd_phys = (uint32_t)kernel_page_directory;
    
    // Load page directory
    load_page_directory(current_pd_phys);
    
    // Enable paging
    enable_paging();
    
    printf("[VMM] Paging enabled! Page directory at 0x%x\n", current_pd_phys);
}

// Create a new page directory for a process
uint32_t vmm_create_page_directory(void) {
    // Allocate page for directory
    uint32_t pd_phys = pmm_alloc_page();
    if (pd_phys == 0) {
        return 0;
    }
    
    pde_t* pd = (pde_t*)pd_phys;
    
    // Clear user space entries
    for (int i = 0; i < 768; i++) {  // First 3GB (user space)
        pd[i] = 0;
    }
    
    // Copy kernel space entries (top 1GB)
    for (int i = 768; i < PAGE_ENTRIES; i++) {
        pd[i] = kernel_page_directory[i];
    }
    
    // Also copy first 4MB identity mapping (needed for now)
    pd[0] = kernel_page_directory[0];
    
    return pd_phys;
}

// Destroy a page directory
void vmm_destroy_page_directory(uint32_t pd_phys) {
    if (pd_phys == 0 || pd_phys == (uint32_t)kernel_page_directory) {
        return;  // Don't destroy kernel page directory
    }
    
    pde_t* pd = (pde_t*)pd_phys;
    
    // Free user space page tables (not kernel ones)
    for (int i = 0; i < 768; i++) {
        if (pd[i] & PTE_PRESENT) {
            uint32_t pt_phys = pd[i] & PTE_FRAME_MASK;
            
            // Free pages in this table
            pte_t* pt = (pte_t*)pt_phys;
            for (int j = 0; j < PAGE_ENTRIES; j++) {
                if (pt[j] & PTE_PRESENT) {
                    pmm_free_page(pt[j] & PTE_FRAME_MASK);
                }
            }
            
            // Free the page table itself
            pmm_free_page(pt_phys);
        }
    }
    
    // Free the page directory
    pmm_free_page(pd_phys);
}

// Switch to a page directory
void vmm_switch_page_directory(uint32_t pd_phys) {
    if (pd_phys != current_pd_phys) {
        current_pd_phys = pd_phys;
        load_page_directory(pd_phys);
    }
}

// Get current page directory
uint32_t vmm_get_current_directory(void) {
    return current_pd_phys;
}

// Map a virtual address to a physical address
bool vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags) {
    pte_t* pt = get_page_table(virt, true);
    if (pt == NULL) {
        return false;
    }
    
    uint32_t pt_index = VADDR_PT_INDEX(virt);
    
    // Check if already mapped
    if (pt[pt_index] & PTE_PRESENT) {
        // Already mapped - update mapping
        invlpg(virt);
    }
    
    // Set up page table entry
    pt[pt_index] = (phys & PTE_FRAME_MASK) | (flags & PTE_FLAGS_MASK) | PTE_PRESENT;
    
    return true;
}

// Unmap a virtual address
void vmm_unmap_page(uint32_t virt) {
    pte_t* pt = get_page_table(virt, false);
    if (pt == NULL) {
        return;
    }
    
    uint32_t pt_index = VADDR_PT_INDEX(virt);
    pt[pt_index] = 0;
    
    invlpg(virt);
}

// Map a range of pages
bool vmm_map_range(uint32_t virt_start, uint32_t phys_start, uint32_t size, uint32_t flags) {
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint32_t i = 0; i < pages; i++) {
        uint32_t virt = virt_start + (i * PAGE_SIZE);
        uint32_t phys = phys_start + (i * PAGE_SIZE);
        
        if (!vmm_map_page(virt, phys, flags)) {
            // Rollback on failure
            vmm_unmap_range(virt_start, i * PAGE_SIZE);
            return false;
        }
    }
    
    return true;
}

// Unmap a range of pages
void vmm_unmap_range(uint32_t virt_start, uint32_t size) {
    uint32_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint32_t i = 0; i < pages; i++) {
        vmm_unmap_page(virt_start + (i * PAGE_SIZE));
    }
}

// Get physical address for a virtual address
uint32_t vmm_get_physical(uint32_t virt) {
    pte_t* pt = get_page_table(virt, false);
    if (pt == NULL) {
        return 0;
    }
    
    uint32_t pt_index = VADDR_PT_INDEX(virt);
    if (!(pt[pt_index] & PTE_PRESENT)) {
        return 0;
    }
    
    return (pt[pt_index] & PTE_FRAME_MASK) | VADDR_OFFSET(virt);
}

// Check if a virtual address is mapped
bool vmm_is_mapped(uint32_t virt) {
    pte_t* pt = get_page_table(virt, false);
    if (pt == NULL) {
        return false;
    }
    
    uint32_t pt_index = VADDR_PT_INDEX(virt);
    return (pt[pt_index] & PTE_PRESENT) != 0;
}

// Allocate and map a page at virtual address
bool vmm_alloc_page(uint32_t virt, uint32_t flags) {
    uint32_t phys = pmm_alloc_page();
    if (phys == 0) {
        return false;
    }
    
    if (!vmm_map_page(virt, phys, flags)) {
        pmm_free_page(phys);
        return false;
    }
    
    // Zero the page
    zero_page(virt);
    
    return true;
}

// Free and unmap a page
void vmm_free_page(uint32_t virt) {
    uint32_t phys = vmm_get_physical(virt);
    if (phys != 0) {
        vmm_unmap_page(virt);
        pmm_free_page(phys);
    }
}

// Page fault handler
void vmm_page_fault_handler(uint32_t fault_addr, uint32_t error_code) {
    printf("\n[VMM] PAGE FAULT at 0x%x\n", fault_addr);
    printf("[VMM] Error code: 0x%x\n", error_code);
    printf("[VMM]   - %s\n", (error_code & 1) ? "Protection violation" : "Page not present");
    printf("[VMM]   - %s access\n", (error_code & 2) ? "Write" : "Read");
    printf("[VMM]   - %s mode\n", (error_code & 4) ? "User" : "Kernel");
    
    // For now, just halt on page fault
    // In a full OS, we would handle this (swap in page, etc.)
    printf("[VMM] FATAL: Unhandled page fault!\n");
    while (1) {
        __asm__ volatile("cli; hlt");
    }
}

// Invalidate TLB entry
void vmm_invalidate_page(uint32_t virt) {
    invlpg(virt);
}

// Flush entire TLB
void vmm_flush_tlb(void) {
    load_page_directory(current_pd_phys);
}

// Debug: print mappings
void vmm_print_mappings(void) {
    printf("[VMM] Page Directory at 0x%x:\n", (uint32_t)kernel_page_directory);
    
    for (int i = 0; i < PAGE_ENTRIES; i++) {
        if (kernel_page_directory[i] & PTE_PRESENT) {
            uint32_t virt_base = i * PAGE_SIZE * PAGE_ENTRIES;
            printf("[VMM]   PDE[%d]: 0x%x -> PT at 0x%x (virt 0x%x - 0x%x)\n",
                   i, kernel_page_directory[i],
                   kernel_page_directory[i] & PTE_FRAME_MASK,
                   virt_base, virt_base + PAGE_SIZE * PAGE_ENTRIES - 1);
        }
    }
}
