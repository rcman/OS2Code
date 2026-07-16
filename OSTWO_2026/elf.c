// File: elf.c
// ELF loader implementation for Linux binary compatibility

#include "elf.h"
#include "process.h"
#include "vmm.h"
#include "pmm.h"

// External functions
extern void printf(const char* format, ...);
extern process_t* process_current(void);
extern uint32_t process_next_pid(void);

// String utilities
static int str_copy_safe(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return i;
}

// Validate ELF header
int elf_validate_header(const Elf32_Ehdr* ehdr) {
    if (!ehdr) {
        printf("[ELF] ERROR: NULL header\n");
        return -1;
    }

    // Check magic number
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        printf("[ELF] ERROR: Invalid magic number\n");
        return -1;
    }

    // Check class (32-bit)
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS32) {
        printf("[ELF] ERROR: Not a 32-bit ELF file\n");
        return -1;
    }

    // Check data encoding (little-endian)
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        printf("[ELF] ERROR: Not little-endian\n");
        return -1;
    }

    // Check version
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        printf("[ELF] ERROR: Invalid ELF version\n");
        return -1;
    }

    // Check machine type (i386)
    if (ehdr->e_machine != EM_386) {
        printf("[ELF] ERROR: Not an i386 executable\n");
        return -1;
    }

    // Check file type (executable or shared object)
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        printf("[ELF] ERROR: Not an executable or shared object (type=%d)\n", ehdr->e_type);
        return -1;
    }

    return 0;
}

// Load ELF segments into memory
// Returns: entry point on success, 0 on failure
static uint32_t elf_load_segments(const void* elf_data, const Elf32_Ehdr* ehdr, uint32_t page_directory) {
    const uint8_t* elf_base = (const uint8_t*)elf_data;

    // Get program header table
    const Elf32_Phdr* phdr_table = (const Elf32_Phdr*)(elf_base + ehdr->e_phoff);

    printf("[ELF] Loading %d program segments...\n", ehdr->e_phnum);

    // Iterate through program headers
    for (int i = 0; i < ehdr->e_phnum; i++) {
        const Elf32_Phdr* phdr = &phdr_table[i];

        // Only load PT_LOAD segments
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        printf("[ELF] Segment %d: vaddr=0x%x, size=0x%x, flags=%c%c%c\n",
               i, phdr->p_vaddr, phdr->p_memsz,
               (phdr->p_flags & PF_R) ? 'R' : '-',
               (phdr->p_flags & PF_W) ? 'W' : '-',
               (phdr->p_flags & PF_X) ? 'X' : '-');

        // Calculate page-aligned addresses
        uint32_t vaddr_start = phdr->p_vaddr & 0xFFFFF000;  // Page-align down
        uint32_t vaddr_end = (phdr->p_vaddr + phdr->p_memsz + 0xFFF) & 0xFFFFF000;  // Page-align up
        uint32_t num_pages = (vaddr_end - vaddr_start) / PAGE_SIZE;

        printf("[ELF] Allocating %d pages from 0x%x to 0x%x\n", num_pages, vaddr_start, vaddr_end);

        // Switch to process's page directory
        uint32_t old_pd = vmm_get_current_directory();
        vmm_switch_page_directory(page_directory);

        // Allocate and map pages for this segment
        for (uint32_t page = 0; page < num_pages; page++) {
            uint32_t virt_addr = vaddr_start + (page * PAGE_SIZE);

            // Allocate physical page
            uint32_t phys_addr = pmm_alloc_page();
            if (phys_addr == 0) {
                printf("[ELF] ERROR: Failed to allocate page\n");
                vmm_switch_page_directory(old_pd);
                return 0;
            }

            // Determine page flags
            uint32_t flags = PTE_PRESENT | PTE_USER;
            if (phdr->p_flags & PF_W) {
                flags |= PTE_WRITABLE;
            }

            // Map page
            if (!vmm_map_page(virt_addr, phys_addr, flags)) {
                printf("[ELF] ERROR: Failed to map page at 0x%x\n", virt_addr);
                pmm_free_page(phys_addr);
                vmm_switch_page_directory(old_pd);
                return 0;
            }

            // Zero the page
            uint8_t* page_ptr = (uint8_t*)virt_addr;
            for (uint32_t j = 0; j < PAGE_SIZE; j++) {
                page_ptr[j] = 0;
            }
        }

        // Copy segment data from ELF file
        if (phdr->p_filesz > 0) {
            const uint8_t* src = elf_base + phdr->p_offset;
            uint8_t* dst = (uint8_t*)phdr->p_vaddr;

            for (uint32_t j = 0; j < phdr->p_filesz; j++) {
                dst[j] = src[j];
            }

            printf("[ELF] Copied %d bytes to 0x%x\n", phdr->p_filesz, phdr->p_vaddr);
        }

        // Zero BSS section (memsz > filesz)
        if (phdr->p_memsz > phdr->p_filesz) {
            uint8_t* bss_start = (uint8_t*)(phdr->p_vaddr + phdr->p_filesz);
            uint32_t bss_size = phdr->p_memsz - phdr->p_filesz;

            for (uint32_t j = 0; j < bss_size; j++) {
                bss_start[j] = 0;
            }

            printf("[ELF] Zeroed %d bytes of BSS at 0x%x\n", bss_size, (uint32_t)bss_start);
        }

        // Switch back to original page directory
        vmm_switch_page_directory(old_pd);
    }

    return ehdr->e_entry;
}

// Load ELF binary from memory
uint32_t elf_load(const void* elf_data, uint32_t size, uint32_t* entry_point) {
    if (!elf_data || size < sizeof(Elf32_Ehdr)) {
        printf("[ELF] ERROR: Invalid ELF data\n");
        return 0;
    }

    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)elf_data;

    // Validate ELF header
    if (elf_validate_header(ehdr) != 0) {
        return 0;
    }

    printf("[ELF] Valid ELF32 executable, entry=0x%x\n", ehdr->e_entry);

    // Create new page directory for process
    uint32_t page_directory = vmm_create_page_directory();
    if (page_directory == 0) {
        printf("[ELF] ERROR: Failed to create page directory\n");
        return 0;
    }

    // Load segments
    uint32_t entry = elf_load_segments(elf_data, ehdr, page_directory);
    if (entry == 0) {
        printf("[ELF] ERROR: Failed to load segments\n");
        vmm_destroy_page_directory(page_directory);
        return 0;
    }

    if (entry_point) {
        *entry_point = entry;
    }

    return page_directory;
}

// Create process from ELF binary
uint32_t elf_exec(const char* name, const void* elf_data, uint32_t size) {
    printf("[ELF] Executing '%s' (%d bytes)\n", name, size);

    // Validate and parse ELF
    if (!elf_data || size < sizeof(Elf32_Ehdr)) {
        printf("[ELF] ERROR: Invalid ELF data\n");
        return 0;
    }

    const Elf32_Ehdr* ehdr = (const Elf32_Ehdr*)elf_data;

    if (elf_validate_header(ehdr) != 0) {
        return 0;
    }

    // Allocate process slot
    extern process_t* process_alloc_slot(void);
    process_t* proc = process_alloc_slot();
    if (!proc) {
        printf("[ELF] ERROR: Process table full\n");
        return 0;
    }

    // Allocate PID
    uint32_t pid = process_next_pid();

    // Set up PCB
    proc->pid = pid;
    proc->state = PROCESS_STATE_READY;
    proc->priority = PRIORITY_REGULAR;
    str_copy_safe(proc->name, name, 32);
    proc->parent_pid = process_current() ? process_current()->pid : 0;
    proc->time_slice = 10;
    proc->total_time = 0;
    proc->exit_code = 0;
    proc->child_count = 0;

    // Initialize memory allocations
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        proc->allocations[i].in_use = 0;
    }

    // Initialize file descriptors
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        proc->file_descriptors[i].in_use = 0;
        proc->file_descriptors[i].node = NULL;
    }

    // Create page directory and load ELF segments
    uint32_t entry_point = 0;
    proc->page_directory = elf_load(elf_data, size, &entry_point);

    if (proc->page_directory == 0 || entry_point == 0) {
        printf("[ELF] ERROR: Failed to load ELF binary\n");
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Allocate kernel stack
    uint32_t kernel_stack_phys = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (kernel_stack_phys == 0) {
        printf("[ELF] ERROR: Failed to allocate kernel stack\n");
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }
    proc->kernel_stack = kernel_stack_phys + KERNEL_STACK_SIZE;

    // Allocate user stack
    uint32_t user_stack_virt = 0xBFFFF000;  // Just below 3GB
    uint32_t user_stack_phys = pmm_alloc_page();
    if (user_stack_phys == 0) {
        printf("[ELF] ERROR: Failed to allocate user stack\n");
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Map user stack
    uint32_t old_pd = vmm_get_current_directory();
    vmm_switch_page_directory(proc->page_directory);

    if (!vmm_map_page(user_stack_virt, user_stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
        printf("[ELF] ERROR: Failed to map user stack\n");
        vmm_switch_page_directory(old_pd);
        pmm_free_page(user_stack_phys);
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    vmm_switch_page_directory(old_pd);

    // Set up process registers
    proc->user_stack = user_stack_virt + PAGE_SIZE - 4;
    proc->eip = entry_point;
    proc->esp = proc->user_stack;
    proc->ebp = proc->user_stack;

    // User mode segments (Ring 3)
    proc->cs = 0x1B;  // User code segment
    proc->ds = proc->es = proc->fs = proc->gs = proc->ss = 0x23;  // User data segment

    // Initial EFLAGS
    proc->eflags = 0x0202;  // IF=1, IOPL=0

    // Clear registers
    proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
    proc->esi = proc->edi = 0;

    printf("[ELF] Created process '%s' (PID %d) entry=0x%x\n", name, pid, entry_point);

    return pid;
}
