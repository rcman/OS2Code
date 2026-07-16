// File: process.c
// Process management implementation

#include "process.h"
#include "vmm.h"
#include "pmm.h"

// External functions
extern void printf(const char* format, ...);
extern void tss_set_kernel_stack(uint32_t stack);

// Process table
static process_t process_table[MAX_PROCESSES];

// Current running process
static process_t* current_process = NULL;

// Next PID to allocate
static uint32_t next_pid = 1;

// External context switch function
extern void process_switch_context(process_t* old_proc, process_t* new_proc);

// String copy helper
static void str_copy_safe(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Initialize process management
void process_init(void) {
    printf("[Process] Initializing process manager...\n");

    // Clear process table
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i].state = PROCESS_STATE_UNUSED;
        process_table[i].pid = 0;
    }

    // Create kernel idle process (PID 0)
    process_table[0].pid = 0;
    process_table[0].state = PROCESS_STATE_RUNNING;
    process_table[0].priority = PRIORITY_IDLE;
    str_copy_safe(process_table[0].name, "kernel_idle", 32);
    process_table[0].page_directory = vmm_get_current_directory();

    current_process = &process_table[0];

    printf("[Process] Created idle process (PID 0)\n");
    printf("[Process] Process manager initialized\n");
}

// Get next available PID
uint32_t process_next_pid(void) {
    return next_pid++;
}

// Allocate a process slot
process_t* process_alloc_slot(void) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state == PROCESS_STATE_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

// Create a new process
uint32_t process_create(const char* name, void (*entry_point)(void), uint32_t priority) {
    // Allocate process slot
    process_t* proc = process_alloc_slot();
    if (!proc) {
        printf("[Process] ERROR: Process table full!\n");
        return 0;
    }

    // Allocate PID
    uint32_t pid = process_next_pid();

    // Set up PCB
    proc->pid = pid;
    proc->state = PROCESS_STATE_READY;
    proc->priority = priority;
    str_copy_safe(proc->name, name, 32);
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->time_slice = 10;  // 10 timer ticks
    proc->total_time = 0;
    proc->exit_code = 0;
    proc->child_count = 0;

    // Initialize memory allocations
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        proc->allocations[i].in_use = 0;
        proc->allocations[i].virtual_addr = 0;
        proc->allocations[i].size_pages = 0;
    }

    // Initialize file descriptors (0=stdin, 1=stdout, 2=stderr)
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        proc->file_descriptors[i].in_use = 0;
        proc->file_descriptors[i].node = NULL;
        proc->file_descriptors[i].position = 0;
        proc->file_descriptors[i].flags = 0;
    }
    // TODO: Set up stdin, stdout, stderr when console VFS node is available

    // Increment parent's child count
    if (proc->parent_pid != 0) {
        process_t* parent = process_get(proc->parent_pid);
        if (parent) {
            parent->child_count++;
        }
    }

    // Create new page directory for this process
    proc->page_directory = vmm_create_page_directory();
    if (proc->page_directory == 0) {
        printf("[Process] ERROR: Failed to create page directory!\n");
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Allocate kernel stack (1 page = 4KB)
    uint32_t kernel_stack_phys = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (kernel_stack_phys == 0) {
        printf("[Process] ERROR: Failed to allocate kernel stack!\n");
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Kernel stack grows down, so stack pointer is at top
    proc->kernel_stack = kernel_stack_phys + KERNEL_STACK_SIZE;

    // Allocate user stack (at 0xC0000000 - 1 page below 3GB mark)
    uint32_t user_stack_virt = 0xBFFFF000;
    uint32_t user_stack_phys = pmm_alloc_page();
    if (user_stack_phys == 0) {
        printf("[Process] ERROR: Failed to allocate user stack!\n");
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Map user stack in process's address space
    // We need to temporarily switch to this process's page directory to map it
    uint32_t old_pd = vmm_get_current_directory();
    vmm_switch_page_directory(proc->page_directory);

    if (!vmm_map_page(user_stack_virt, user_stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
        printf("[Process] ERROR: Failed to map user stack!\n");
        vmm_switch_page_directory(old_pd);
        pmm_free_page(user_stack_phys);
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Map the code page (where entry_point is). Kernel test functions
    // live in the identity-mapped first 4MB, which is already mapped
    // user-accessible in the shared kernel page table, so no extra
    // mapping is needed (and mapping over the shared identity region
    // is now refused by vmm_map_page).
    uint32_t code_page = ((uint32_t)entry_point) & 0xFFFFF000;
    if (code_page >= 0x40000000) {
        uint32_t code_phys = vmm_get_physical(code_page);
        if (code_phys == 0) {
            code_phys = code_page;
        }
        vmm_map_page(code_page, code_phys, PTE_PRESENT | PTE_USER);
    }

    // Switch back to original page directory
    vmm_switch_page_directory(old_pd);

    // Stack grows down - set to last valid address in page (not beyond it)
    proc->user_stack = user_stack_virt + PAGE_SIZE - 4;

    // Initialize CPU context
    proc->eip = (uint32_t)entry_point;
    proc->esp = proc->user_stack;
    proc->ebp = proc->user_stack;

    // User mode segment selectors (Ring 3)
    proc->cs = 0x1B;  // User code segment (0x18 | 0x03)
    proc->ds = proc->es = proc->fs = proc->gs = proc->ss = 0x23;  // User data (0x20 | 0x03)

    // Initial EFLAGS (interrupts enabled, IOPL=0 for security)
    // IOPL=0 prevents user mode from executing I/O instructions (IN/OUT)
    // User mode must use syscalls for all I/O operations
    proc->eflags = 0x0202;  // IF flag + IOPL=0

    // Clear general purpose registers
    proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
    proc->esi = proc->edi = 0;

    printf("[Process] Created process '%s' (PID %d) at 0x%x\n", name, pid, entry_point);

    return pid;
}

// Create a new process from binary data (for DosExecPgm)
uint32_t process_create_from_binary(const char* name, const void* binary, uint32_t size, uint32_t priority) {
    // Allocate process slot
    process_t* proc = process_alloc_slot();
    if (!proc) {
        printf("[Process] ERROR: Process table full!\n");
        return 0;
    }

    // Validate size
    if (size == 0 || size > PAGE_SIZE) {
        printf("[Process] ERROR: Binary size %d invalid (max %d)\n", size, PAGE_SIZE);
        return 0;
    }

    // Allocate PID
    uint32_t pid = process_next_pid();

    // Set up PCB
    proc->pid = pid;
    proc->state = PROCESS_STATE_READY;
    proc->priority = priority;
    str_copy_safe(proc->name, name, 32);
    proc->parent_pid = current_process ? current_process->pid : 0;
    proc->time_slice = 10;
    proc->total_time = 0;
    proc->exit_code = 0;
    proc->child_count = 0;

    // Initialize memory allocations
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        proc->allocations[i].in_use = 0;
        proc->allocations[i].virtual_addr = 0;
        proc->allocations[i].size_pages = 0;
    }

    // Initialize file descriptors (0=stdin, 1=stdout, 2=stderr)
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        proc->file_descriptors[i].in_use = 0;
        proc->file_descriptors[i].node = NULL;
        proc->file_descriptors[i].position = 0;
        proc->file_descriptors[i].flags = 0;
    }

    // Increment parent's child count
    if (proc->parent_pid != 0) {
        process_t* parent = process_get(proc->parent_pid);
        if (parent) {
            parent->child_count++;
        }
    }

    // Create new page directory for this process
    proc->page_directory = vmm_create_page_directory();
    if (proc->page_directory == 0) {
        printf("[Process] ERROR: Failed to create page directory!\n");
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Allocate kernel stack (1 page = 4KB)
    uint32_t kernel_stack_phys = pmm_alloc_pages(KERNEL_STACK_PAGES);
    if (kernel_stack_phys == 0) {
        printf("[Process] ERROR: Failed to allocate kernel stack!\n");
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    proc->kernel_stack = kernel_stack_phys + KERNEL_STACK_SIZE;

    // Allocate user stack (at 0xBFFFF000)
    uint32_t user_stack_virt = 0xBFFFF000;
    uint32_t user_stack_phys = pmm_alloc_page();
    if (user_stack_phys == 0) {
        printf("[Process] ERROR: Failed to allocate user stack!\n");
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Allocate code page for the binary (load at USER_LOAD_ADDR, above
    // the kernel identity-mapped RAM region)
    uint32_t code_virt = 0x40000000;
    uint32_t code_phys = pmm_alloc_page();
    if (code_phys == 0) {
        printf("[Process] ERROR: Failed to allocate code page!\n");
        pmm_free_page(user_stack_phys);
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Copy binary data to the physical page BEFORE switching page directories
    // The kernel identity-maps the first 4MB, so we can access the physical page directly
    uint8_t* code_dest_phys = (uint8_t*)code_phys;
    const uint8_t* code_src = (const uint8_t*)binary;
    for (uint32_t i = 0; i < size; i++) {
        code_dest_phys[i] = code_src[i];
    }
    // Zero out the rest of the page
    for (uint32_t i = size; i < PAGE_SIZE; i++) {
        code_dest_phys[i] = 0;
    }

    // Switch to process's page directory to set up its memory
    uint32_t old_pd = vmm_get_current_directory();
    vmm_switch_page_directory(proc->page_directory);

    // Map user stack
    if (!vmm_map_page(user_stack_virt, user_stack_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
        printf("[Process] ERROR: Failed to map user stack!\n");
        vmm_switch_page_directory(old_pd);
        pmm_free_page(code_phys);
        pmm_free_page(user_stack_phys);
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Map code page as readable and executable
    if (!vmm_map_page(code_virt, code_phys, PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
        printf("[Process] ERROR: Failed to map code page!\n");
        vmm_switch_page_directory(old_pd);
        pmm_free_page(code_phys);
        pmm_free_page(user_stack_phys);
        pmm_free_pages(kernel_stack_phys, KERNEL_STACK_PAGES);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Switch back to original page directory
    vmm_switch_page_directory(old_pd);

    // Set up process registers
    proc->user_stack = user_stack_virt + PAGE_SIZE - 4;
    proc->eip = code_virt;  // Entry point is start of code page
    proc->esp = proc->user_stack;
    proc->ebp = proc->user_stack;

    // User mode segment selectors (Ring 3)
    proc->cs = 0x1B;
    proc->ds = proc->es = proc->fs = proc->gs = proc->ss = 0x23;

    // Initial EFLAGS (interrupts enabled, IOPL=0)
    proc->eflags = 0x0202;

    // Clear general purpose registers
    proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
    proc->esi = proc->edi = 0;

    printf("[Process] Created process '%s' (PID %d) from binary (%d bytes at 0x%x)\n",
           name, pid, size, code_virt);

    return pid;
}

// Terminate a process with exit code
void process_exit_with_code(uint32_t pid, uint32_t exit_code) {
    process_t* proc = process_get(pid);
    if (!proc || proc->state == PROCESS_STATE_UNUSED) {
        return;
    }

    printf("[Process] Terminating process '%s' (PID %d) with exit code %d\n",
           proc->name, pid, exit_code);

    // Save exit code
    proc->exit_code = exit_code;

    // Decrement parent's child count
    if (proc->parent_pid != 0) {
        process_t* parent = process_get(proc->parent_pid);
        if (parent && parent->child_count > 0) {
            parent->child_count--;
            printf("[Process] Parent PID %d now has %d children\n",
                   proc->parent_pid, parent->child_count);
        }
    }

    // Reparent orphaned children to init (PID 0)
    if (proc->child_count > 0) {
        printf("[Process] Reparenting %d orphaned children to init\n", proc->child_count);
        for (int i = 0; i < MAX_PROCESSES; i++) {
            process_t* child = &process_table[i];
            if (child->state != PROCESS_STATE_UNUSED && child->parent_pid == pid) {
                child->parent_pid = 0;  // Adopt by init
            }
        }
    }

    // Mark as terminated (but don't free page directory yet if it's current)
    proc->state = PROCESS_STATE_TERMINATED;

    // If this is NOT the current process, we can free resources now
    if (proc != current_process) {
        if (proc->page_directory != 0) {
            vmm_destroy_page_directory(proc->page_directory);
        }
        proc->state = PROCESS_STATE_UNUSED;
        proc->pid = 0;
    } else {
        // Current process - mark as terminated and let scheduler switch away
        // Page directory will be cleaned up later (or kept for now)
        // Setting pid = 0 would break the scheduler, so keep it for now
        printf("[Process] Process %d terminated, will be cleaned after context switch\n", pid);
    }
}

// Terminate a process (wrapper for compatibility)
void process_exit(uint32_t pid) {
    process_exit_with_code(pid, 0);
}

// Reap all TERMINATED processes: free their page directory and
// kernel stack, and release the process table slot. Must be called
// from kernel context, on the kernel page directory, with the
// process no longer current. Fixes the per-exec memory leak where
// the current process's resources were never freed.
void process_reap_terminated(void) {
    for (int i = 1; i < MAX_PROCESSES; i++) {   // never reap PID 0 (idle)
        process_t* proc = &process_table[i];
        if (proc->state != PROCESS_STATE_TERMINATED) continue;
        if (proc == current_process) continue;   // still running somehow

        if (proc->page_directory != 0 &&
            proc->page_directory != vmm_get_current_directory()) {
            vmm_destroy_page_directory(proc->page_directory);
            proc->page_directory = 0;
        }

        if (proc->kernel_stack != 0) {
            // kernel_stack points at the TOP of a 1-page stack
            pmm_free_page(proc->kernel_stack - PAGE_SIZE);
            proc->kernel_stack = 0;
        }

        proc->state = PROCESS_STATE_UNUSED;
        proc->pid = 0;
        proc->name[0] = '\0';
    }
}

// Get current running process
process_t* process_current(void) {
    return current_process;
}

// Get PID from process pointer (for syscall use)
uint32_t process_get_pid(process_t* proc) {
    return proc ? proc->pid : 0;
}

// Get process by PID
process_t* process_get(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].pid == pid && process_table[i].state != PROCESS_STATE_UNUSED) {
            return &process_table[i];
        }
    }
    return NULL;
}

// Set current process (used by scheduler)
void process_set_current(process_t* proc) {
    current_process = proc;

    // Update TSS with this process's kernel stack
    if (proc) {
        tss_set_kernel_stack(proc->kernel_stack);
    }
}

// Get process table for iteration
process_t* process_get_table(void) {
    return process_table;
}

// Get max processes
int process_get_max(void) {
    return MAX_PROCESSES;
}

// Get parent process ID
uint32_t process_get_ppid(process_t* proc) {
    if (proc) {
        return proc->parent_pid;
    }
    return 0;
}

// Set process priority
void process_set_priority(process_t* proc, uint32_t priority) {
    if (proc && priority <= PRIORITY_REALTIME) {
        proc->priority = priority;
    }
}

// Yield CPU (called by scheduler)
void process_yield(void) {
    // This will be called by the scheduler
    // For now, just a placeholder
}

// Print process table
void process_print_table(void) {
    printf("PID  State      Priority  Parent  Children  Name\n");
    printf("---  ---------  --------  ------  --------  ----\n");

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i].state != PROCESS_STATE_UNUSED) {
            const char* state_str;
            switch (process_table[i].state) {
                case PROCESS_STATE_READY:      state_str = "READY    "; break;
                case PROCESS_STATE_RUNNING:    state_str = "RUNNING  "; break;
                case PROCESS_STATE_BLOCKED:    state_str = "BLOCKED  "; break;
                case PROCESS_STATE_TERMINATED: state_str = "TERMINATED"; break;
                default:                       state_str = "UNKNOWN  "; break;
            }

            printf("%-4d %s %-8d  %-6d  %-8d  %s\n",
                   process_table[i].pid,
                   state_str,
                   process_table[i].priority,
                   process_table[i].parent_pid,
                   process_table[i].child_count,
                   process_table[i].name);
        }
    }
}

// Get parent PID of a process
uint32_t process_get_parent(uint32_t pid) {
    process_t* proc = process_get(pid);
    return proc ? proc->parent_pid : 0;
}

// Get number of children
uint32_t process_get_child_count(uint32_t pid) {
    process_t* proc = process_get(pid);
    return proc ? proc->child_count : 0;
}

// List all child PIDs (returns count, fills array)
uint32_t process_list_children(uint32_t pid, uint32_t* child_pids, uint32_t max) {
    uint32_t count = 0;
    for (int i = 0; i < MAX_PROCESSES && count < max; i++) {
        if (process_table[i].state != PROCESS_STATE_UNUSED &&
            process_table[i].parent_pid == pid) {
            child_pids[count++] = process_table[i].pid;
        }
    }
    return count;
}

// Allocate memory for a process
// Returns virtual address on success, NULL on failure
void* process_alloc_mem(process_t* proc, uint32_t size_bytes) {
    if (!proc || size_bytes == 0) {
        return NULL;
    }

    // Calculate number of pages needed (round up)
    uint32_t num_pages = (size_bytes + 4095) / 4096;

    // Find free allocation slot
    int slot = -1;
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        if (!proc->allocations[i].in_use) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("[Process] ERROR: No free allocation slots for PID %d\n", proc->pid);
        return NULL;
    }

    // Choose virtual address for this allocation
    // Start at 0x10000000 (256MB) and allocate upwards
    // Each allocation gets its own range
    uint32_t base_virt = 0x10000000 + (slot * 0x100000);  // 1MB per slot

    // Save current page directory and switch to process's page directory
    extern uint32_t vmm_get_current_directory(void);
    extern void vmm_switch_page_directory(uint32_t pd_phys);
    uint32_t saved_pd = vmm_get_current_directory();
    vmm_switch_page_directory(proc->page_directory);

    // Allocate and map pages
    for (uint32_t i = 0; i < num_pages; i++) {
        uint32_t phys_page = pmm_alloc_page();
        if (phys_page == 0) {
            // Out of memory - free what we allocated so far
            for (uint32_t j = 0; j < i; j++) {
                uint32_t virt = base_virt + (j * 4096);
                uint32_t phys = vmm_get_physical(virt);
                if (phys) {
                    vmm_unmap_page(virt);
                    pmm_free_page(phys);
                }
            }
            printf("[Process] ERROR: Out of physical memory\n");
            vmm_switch_page_directory(saved_pd);
            return NULL;
        }

        uint32_t virt_addr = base_virt + (i * 4096);

        // Map with user permissions (PTE_USER | PTE_WRITABLE | PTE_PRESENT)
        extern bool vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
        if (!vmm_map_page(virt_addr, phys_page,
                          PTE_PRESENT | PTE_WRITABLE | PTE_USER)) {
            // Mapping failed - clean up
            pmm_free_page(phys_page);
            for (uint32_t j = 0; j < i; j++) {
                uint32_t virt = base_virt + (j * 4096);
                uint32_t phys = vmm_get_physical(virt);
                if (phys) {
                    vmm_unmap_page(virt);
                    pmm_free_page(phys);
                }
            }
            printf("[Process] ERROR: Failed to map page\n");
            vmm_switch_page_directory(saved_pd);
            return NULL;
        }
    }

    // Switch back to saved page directory
    vmm_switch_page_directory(saved_pd);

    // Record allocation
    proc->allocations[slot].virtual_addr = base_virt;
    proc->allocations[slot].size_pages = num_pages;
    proc->allocations[slot].in_use = 1;

    printf("[Process] Allocated %d bytes (%d pages) at 0x%x for PID %d\n",
           size_bytes, num_pages, base_virt, proc->pid);

    return (void*)base_virt;
}

// Free memory allocated by a process
// Returns 0 on success, -1 on failure
int process_free_mem(process_t* proc, void* addr) {
    if (!proc || !addr) {
        return -1;
    }

    uint32_t virt_addr = (uint32_t)addr;

    // Find the allocation
    int slot = -1;
    for (int i = 0; i < MAX_ALLOCATIONS; i++) {
        if (proc->allocations[i].in_use &&
            proc->allocations[i].virtual_addr == virt_addr) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        printf("[Process] ERROR: Invalid address 0x%x for PID %d\n", virt_addr, proc->pid);
        return -1;
    }

    // Save current page directory and switch to process's page directory
    extern uint32_t vmm_get_current_directory(void);
    extern void vmm_switch_page_directory(uint32_t pd_phys);
    uint32_t saved_pd = vmm_get_current_directory();
    vmm_switch_page_directory(proc->page_directory);

    // Unmap and free all pages
    for (uint32_t i = 0; i < proc->allocations[slot].size_pages; i++) {
        uint32_t virt = virt_addr + (i * 4096);
        uint32_t phys = vmm_get_physical(virt);
        if (phys) {
            vmm_unmap_page(virt);
            pmm_free_page(phys);
        }
    }

    // Switch back to saved page directory
    vmm_switch_page_directory(saved_pd);

    printf("[Process] Freed %d pages at 0x%x for PID %d\n",
           proc->allocations[slot].size_pages, virt_addr, proc->pid);

    // Mark slot as free
    proc->allocations[slot].in_use = 0;
    proc->allocations[slot].virtual_addr = 0;
    proc->allocations[slot].size_pages = 0;

    return 0;
}
