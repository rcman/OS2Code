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
static process_t* process_alloc(void) {
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
    process_t* proc = process_alloc();
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
    uint32_t kernel_stack_phys = pmm_alloc_page();
    if (kernel_stack_phys == 0) {
        printf("[Process] ERROR: Failed to allocate kernel stack!\n");
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Kernel stack grows down, so stack pointer is at top
    proc->kernel_stack = kernel_stack_phys + PAGE_SIZE;

    // Allocate user stack (at 0xC0000000 - 1 page below 3GB mark)
    uint32_t user_stack_virt = 0xBFFFF000;
    uint32_t user_stack_phys = pmm_alloc_page();
    if (user_stack_phys == 0) {
        printf("[Process] ERROR: Failed to allocate user stack!\n");
        pmm_free_page(kernel_stack_phys);
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
        pmm_free_page(kernel_stack_phys);
        vmm_destroy_page_directory(proc->page_directory);
        proc->state = PROCESS_STATE_UNUSED;
        return 0;
    }

    // Map the code page (where entry_point is)
    uint32_t code_page = ((uint32_t)entry_point) & 0xFFFFF000;
    uint32_t code_phys = vmm_get_physical(code_page);
    if (code_phys == 0) {
        // Code is in identity-mapped region, just use the virtual address
        code_phys = code_page;
    }
    vmm_map_page(code_page, code_phys, PTE_PRESENT | PTE_USER);

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

    // Initial EFLAGS (interrupts enabled, IOPL=3 for I/O access)
    // TODO: Set IOPL=0 and use syscalls only (requires fixing syscall return bug)
    proc->eflags = 0x3202;  // IF flag + IOPL=3

    // Clear general purpose registers
    proc->eax = proc->ebx = proc->ecx = proc->edx = 0;
    proc->esi = proc->edi = 0;

    printf("[Process] Created process '%s' (PID %d) at 0x%x\n", name, pid, entry_point);

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

    // Free resources
    if (proc->page_directory != 0 && proc->page_directory != vmm_get_current_directory()) {
        vmm_destroy_page_directory(proc->page_directory);
    }

    // Mark as unused
    proc->state = PROCESS_STATE_UNUSED;
    proc->pid = 0;

    // If this is the current process, yield to scheduler
    if (proc == current_process) {
        process_yield();
    }
}

// Terminate a process (wrapper for compatibility)
void process_exit(uint32_t pid) {
    process_exit_with_code(pid, 0);
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
