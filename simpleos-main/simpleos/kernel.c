// File: kernel.c
// Main kernel entry point - SimpleOS v0.3 with Virtual Memory

#include "types.h"
#include "vga.h"
#include "events.h"
#include "ramfs.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"

// External functions from other modules
extern void gdt_init(void);
extern void idt_init(void);
extern void timer_init(uint32_t frequency);
extern void keyboard_init(void);
extern void mouse_init(void);
extern void syscall_init(void);
extern void graphics_init(void);
extern void enable_interrupts(void);
extern void disable_interrupts(void);
extern void halt_cpu(void);
extern uint32_t timer_get_ticks(void);
extern void serial_init(void);
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void enter_user_mode(void);
extern void process_init(void);
extern void scheduler_init(void);
extern void scheduler_start(void);
extern uint32_t process_create(const char* name, void (*entry_point)(void), uint32_t priority);
extern void process_print_table(void);
extern void test_process_1(void);
extern void test_process_2(void);
extern void test_process_3(void);
extern void test_process_4(void);
extern void test_parent_process(void);
extern void test_child_process(void);

// Printf
extern void printf(const char* format, ...);

// Kernel end symbol from linker
extern uint32_t _kernel_end;

// Multiboot constants
#define MULTIBOOT_MAGIC 0x2BADB002

// Multiboot flags
#define MB_FLAG_MEM     (1 << 0)    // mem_* fields valid
#define MB_FLAG_BOOT    (1 << 1)    // boot_device valid
#define MB_FLAG_CMDLINE (1 << 2)    // cmdline valid
#define MB_FLAG_MODS    (1 << 3)    // modules valid
#define MB_FLAG_MMAP    (1 << 6)    // mmap_* fields valid

// Multiboot information structure
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;
    uint32_t mmap_addr;
    uint32_t drives_length;
    uint32_t drives_addr;
    uint32_t config_table;
    uint32_t boot_loader_name;
} __attribute__((packed)) multiboot_info_t;

// Simple shell
static void shell_prompt(void);
static void shell_process_command(const char* cmd);

// Command buffer
#define CMD_BUFFER_SIZE 256
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_pos = 0;

// String utilities
static int str_starts_with(const char* str, const char* prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++;
        prefix++;
    }
    return 1;
}

static int str_equal(const char* a, const char* b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++;
        b++;
    }
    return *a == *b;
}

static const char* skip_whitespace(const char* s) {
    while (*s == ' ' || *s == '\t') s++;
    return s;
}

static const char* skip_to_whitespace(const char* s) {
    while (*s && *s != ' ' && *s != '\t') s++;
    return s;
}

static int str_len(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

static void str_copy(char* dest, const char* src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

// Callback for listing files
static void list_file_callback(const char* name, uint32_t size) {
    printf("  %-20s %5d bytes\n", name, size);
}

// Shell prompt
static void shell_prompt(void) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    printf("kernel> ");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

// Page fault handler (ISR 14)
static void page_fault_handler(registers_t* regs) {
    uint32_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));
    vmm_page_fault_handler(fault_addr, regs->err_code);
}

// Process a command
static void shell_process_command(const char* cmd) {
    cmd = skip_whitespace(cmd);
    
    if (cmd[0] == '\0') {
        return;
    }
    
    // === Basic commands ===
    if (str_equal(cmd, "help")) {
        printf("Available commands:\n");
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        printf("\n  System:\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        printf("    help              - Show this help\n");
        printf("    clear             - Clear screen\n");
        printf("    info              - Show system info\n");
        printf("    mem               - Show memory info\n");
        printf("    vmm               - Show VMM mappings\n");
        printf("    alloc             - Test page allocation\n");
        printf("    ps                - Show process table\n");
        printf("    testproc          - Create and run test processes\n");
        printf("    testexit          - Test process termination (DosExit)\n");
        printf("    testhier          - Test parent-child hierarchy\n");
        printf("    usermode          - Test user mode (Ring 3)\n");
        printf("    reboot            - Reboot the system\n");
        printf("    halt              - Halt the CPU\n");
        vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
        printf("\n  Filesystem (RamFS):\n");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        printf("    ls                - List all files\n");
        printf("    touch <file>      - Create empty file\n");
        printf("    write <f> <txt>   - Write text to file\n");
        printf("    cat <file>        - Display file contents\n");
        printf("    rm <file>         - Delete file\n");
        printf("    df                - Show filesystem info\n");
    }
    else if (str_equal(cmd, "clear")) {
        vga_clear();
    }
    else if (str_equal(cmd, "info")) {
        printf("SimpleOS v0.3 (Virtual Memory)\n");
        printf("Architecture: i386 (32-bit)\n");
        printf("Timer frequency: 100 Hz\n");
        printf("Current ticks: %d\n", timer_get_ticks());
        printf("Files in RamFS: %d\n", ramfs_count());
    }
    else if (str_equal(cmd, "mem")) {
        pmm_print_stats();
    }
    else if (str_equal(cmd, "vmm")) {
        vmm_print_mappings();
    }
    else if (str_equal(cmd, "alloc")) {
        printf("Testing page allocation...\n");
        
        // Allocate a page
        uint32_t phys = pmm_alloc_page();
        if (phys) {
            printf("  Allocated physical page at 0x%x\n", phys);
            
            // Map it at a test virtual address
            uint32_t virt = 0x400000;  // 4MB
            if (vmm_map_page(virt, phys, PTE_WRITABLE)) {
                printf("  Mapped to virtual address 0x%x\n", virt);
                
                // Write to it
                uint32_t* ptr = (uint32_t*)virt;
                *ptr = 0xDEADBEEF;
                printf("  Wrote 0xDEADBEEF to page\n");
                
                // Read back
                printf("  Read back: 0x%x\n", *ptr);
                
                // Unmap
                vmm_unmap_page(virt);
                printf("  Unmapped virtual address\n");
            }
            
            // Free the page
            pmm_free_page(phys);
            printf("  Freed physical page\n");
        } else {
            printf("  ERROR: Could not allocate page!\n");
        }
        
        pmm_print_stats();
    }
    else if (str_equal(cmd, "reboot")) {
        printf("Rebooting...\n");
        disable_interrupts();
        uint8_t tmp[6] = {0};
        __asm__ volatile("lidt %0" : : "m"(tmp));
        __asm__ volatile("int $0x03");
    }
    else if (str_equal(cmd, "halt")) {
        printf("Halting CPU...\n");
        disable_interrupts();
        while (1) {
            halt_cpu();
        }
    }
    else if (str_equal(cmd, "ps")) {
        process_print_table();
    }
    else if (str_equal(cmd, "testproc")) {
        printf("Creating test processes...\n");

        // Create 3 test processes
        uint32_t pid1 = process_create("TestA", test_process_1, PRIORITY_REGULAR);
        uint32_t pid2 = process_create("TestB", test_process_2, PRIORITY_REGULAR);
        uint32_t pid3 = process_create("TestC", test_process_3, PRIORITY_REGULAR);

        if (pid1 && pid2 && pid3) {
            printf("Created processes: PID %d, %d, %d\n", pid1, pid2, pid3);
            printf("Starting scheduler...\n");
            printf("You should see A, B, C printed in rotation.\n");
            printf("(Press Ctrl+C or reset to stop)\n\n");

            scheduler_start();

            // The scheduler will now take over
        } else {
            printf("ERROR: Failed to create test processes!\n");
        }
    }
    else if (str_equal(cmd, "testexit")) {
        printf("Testing DosExit - process termination...\n");

        // Create finite process that exits
        uint32_t pid1 = process_create("TestX", test_process_4, PRIORITY_REGULAR);
        // Create 2 infinite processes
        uint32_t pid2 = process_create("TestA", test_process_1, PRIORITY_REGULAR);
        uint32_t pid3 = process_create("TestB", test_process_2, PRIORITY_REGULAR);

        if (pid1 && pid2 && pid3) {
            printf("Created: TestX (PID %d, will exit), TestA (PID %d), TestB (PID %d)\n",
                   pid1, pid2, pid3);
            printf("Watch: X will print 5 times then exit, leaving A and B running.\n\n");
            scheduler_start();
        } else {
            printf("ERROR: Failed to create test processes!\n");
        }
    }
    else if (str_equal(cmd, "testhier")) {
        printf("Testing parent-child process hierarchy...\n");

        // Create parent process
        uint32_t parent_pid = process_create("Parent", test_parent_process, PRIORITY_REGULAR);

        // Create 2 child processes (will have parent_pid set)
        uint32_t child1_pid = process_create("Child1", test_child_process, PRIORITY_REGULAR);
        uint32_t child2_pid = process_create("Child2", test_child_process, PRIORITY_REGULAR);

        // Create one background process (infinite)
        uint32_t bg_pid = process_create("TestA", test_process_1, PRIORITY_REGULAR);

        if (parent_pid && child1_pid && child2_pid && bg_pid) {
            printf("Created hierarchy:\n");
            printf("  Parent (PID %d) - will exit after 10 iterations\n", parent_pid);
            printf("  Child1 (PID %d) - will exit with code 42 after 3 iterations\n", child1_pid);
            printf("  Child2 (PID %d) - will exit with code 42 after 3 iterations\n", child2_pid);
            printf("  TestA  (PID %d) - background process (infinite)\n", bg_pid);
            printf("\nRun 'ps' later to see parent/child relationships and exit codes.\n\n");
            scheduler_start();
        } else {
            printf("ERROR: Failed to create test processes!\n");
        }
    }
    else if (str_equal(cmd, "usermode")) {
        printf("Testing user mode (Ring 3) support...\n");
        printf("This will jump to Ring 3 and make a system call.\n");
        printf("If successful, you'll see a syscall message.\n\n");
        enter_user_mode();
    }
    // === Filesystem commands ===
    else if (str_equal(cmd, "ls")) {
        int count = ramfs_count();
        if (count == 0) {
            printf("No files in filesystem.\n");
        } else {
            printf("Files in RamFS (%d):\n", count);
            ramfs_list(list_file_callback);
        }
    }
    else if (str_equal(cmd, "df")) {
        printf("RamFS Filesystem Info:\n");
        printf("  Max files:     %d\n", RAMFS_MAX_FILES);
        printf("  Used files:    %d\n", ramfs_count());
        printf("  Free slots:    %d\n", RAMFS_MAX_FILES - ramfs_count());
        printf("  Max file size: %d bytes\n", RAMFS_MAX_FILESIZE);
        printf("  Free space:    %d bytes\n", ramfs_free_space());
    }
    else if (str_starts_with(cmd, "touch ")) {
        const char* name = skip_whitespace(cmd + 6);
        if (*name == '\0') {
            printf("Usage: touch <filename>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* end = skip_to_whitespace(name);
            int len = end - name;
            if (len >= RAMFS_MAX_FILENAME) len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, name, len + 1);
            
            if (ramfs_exists(filename)) {
                printf("File '%s' already exists.\n", filename);
            } else if (ramfs_create(filename) == 0) {
                printf("Created file '%s'\n", filename);
            } else {
                printf("Error: Could not create file.\n");
            }
        }
    }
    else if (str_starts_with(cmd, "rm ")) {
        const char* name = skip_whitespace(cmd + 3);
        if (*name == '\0') {
            printf("Usage: rm <filename>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* end = skip_to_whitespace(name);
            int len = end - name;
            if (len >= RAMFS_MAX_FILENAME) len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, name, len + 1);
            
            if (ramfs_delete(filename) == 0) {
                printf("Deleted '%s'\n", filename);
            } else {
                printf("Error: File '%s' not found.\n", filename);
            }
        }
    }
    else if (str_starts_with(cmd, "cat ")) {
        const char* name = skip_whitespace(cmd + 4);
        if (*name == '\0') {
            printf("Usage: cat <filename>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* end = skip_to_whitespace(name);
            int len = end - name;
            if (len >= RAMFS_MAX_FILENAME) len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, name, len + 1);
            
            if (!ramfs_exists(filename)) {
                printf("Error: File '%s' not found.\n", filename);
            } else {
                char buffer[RAMFS_MAX_FILESIZE + 1];
                int bytes = ramfs_read(filename, buffer, RAMFS_MAX_FILESIZE);
                if (bytes >= 0) {
                    buffer[bytes] = '\0';
                    if (bytes == 0) {
                        printf("(empty file)\n");
                    } else {
                        printf("%s", buffer);
                        if (bytes > 0 && buffer[bytes-1] != '\n') {
                            printf("\n");
                        }
                    }
                }
            }
        }
    }
    else if (str_starts_with(cmd, "write ")) {
        const char* args = skip_whitespace(cmd + 6);
        if (*args == '\0') {
            printf("Usage: write <filename> <text>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* name_end = skip_to_whitespace(args);
            int name_len = name_end - args;
            if (name_len >= RAMFS_MAX_FILENAME) name_len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, args, name_len + 1);
            
            const char* content = skip_whitespace(name_end);
            if (*content == '\0') {
                printf("Usage: write <filename> <text>\n");
            } else {
                int content_len = str_len(content);
                int written = ramfs_write(filename, content, content_len);
                if (written >= 0) {
                    printf("Wrote %d bytes to '%s'\n", written, filename);
                } else {
                    printf("Error: Could not write to file.\n");
                }
            }
        }
    }
    else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for available commands.\n");
    }
}

// Main kernel entry point
void kmain(uint32_t magic, multiboot_info_t* mbi) {
    // Initialize serial port first (for debugging)
    serial_init();
    
    // Initialize VGA text mode
    vga_init();
    
    // Print banner
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    printf("=====================================\n");
    printf("  SimpleOS v0.3 (Virtual Memory)\n");
    printf("=====================================\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Get kernel end address
    uint32_t kernel_end = (uint32_t)&_kernel_end;
    printf("[Boot] Kernel ends at 0x%x\n", kernel_end);
    
    // Check multiboot magic and info
    bool have_mmap = false;
    if (magic != MULTIBOOT_MAGIC) {
        printf("[Boot] WARNING: Invalid multiboot magic (0x%x)\n", magic);
        printf("[Boot] Assuming 64MB RAM for testing\n");
    } else {
        printf("[Boot] Multiboot magic OK (0x%x)\n", magic);
        printf("[Boot] Multiboot flags: 0x%x\n", mbi->flags);
        
        if (mbi->flags & MB_FLAG_MEM) {
            printf("[Boot] Memory: %d KB lower, %d KB upper\n", 
                   mbi->mem_lower, mbi->mem_upper);
        }
        
        if (mbi->flags & MB_FLAG_MMAP) {
            printf("[Boot] Memory map available at 0x%x, length %d\n",
                   mbi->mmap_addr, mbi->mmap_length);
            have_mmap = true;
        }
    }
    
    // Initialize GDT
    printf("[Init] Setting up GDT...\n");
    gdt_init();
    printf("[Init] GDT loaded\n");
    
    // Initialize IDT and PIC
    printf("[Init] Setting up IDT...\n");
    idt_init();
    printf("[Init] IDT loaded, PIC remapped\n");
    
    // Register page fault handler BEFORE enabling paging
    register_interrupt_handler(14, page_fault_handler);
    
    // Initialize Physical Memory Manager
    printf("[Init] Setting up PMM...\n");
    if (have_mmap) {
        pmm_init(mbi->mmap_addr, mbi->mmap_length, kernel_end);
    } else {
        // Create a fake memory map for 64MB
        // This is a fallback when multiboot doesn't provide memory map
        printf("[PMM] No memory map, using fallback (64MB)\n");
        
        // Manually mark regions
        // For now, we'll just set up a simple 64MB region
        // This is simplified - real code would need proper mmap handling
        static struct {
            uint32_t size;
            uint64_t base;
            uint64_t length;
            uint32_t type;
        } __attribute__((packed)) fake_mmap[2] = {
            { 20, 0x0, 0x100000, 2 },           // First 1MB reserved
            { 20, 0x100000, 64*1024*1024 - 0x100000, 1 }  // Rest available
        };
        pmm_init((uint32_t)fake_mmap, sizeof(fake_mmap), kernel_end);
    }
    
    // Initialize Virtual Memory Manager
    printf("[Init] Setting up VMM...\n");
    vmm_init();
    
    // Initialize timer (100 Hz)
    timer_init(100);
    
    // Initialize keyboard
    keyboard_init();
    
    // Initialize mouse
    mouse_init();
    
    // Initialize syscalls
    syscall_init();
    
    // Initialize graphics (stays in text mode)
    graphics_init();
    
    // Initialize RAM filesystem
    printf("[Init] Initializing RamFS...\n");
    ramfs_init();
    printf("[RamFS] Initialized (%d file slots, %d bytes/file)\n",
           RAMFS_MAX_FILES, RAMFS_MAX_FILESIZE);

    // Initialize process management
    printf("[Init] Initializing process manager...\n");
    process_init();

    // Initialize scheduler
    printf("[Init] Initializing scheduler...\n");
    scheduler_init();
    
    // Create welcome files
    ramfs_write("welcome.txt", "Welcome to SimpleOS v0.3!\nNow with virtual memory support.\n", 58);
    ramfs_write("readme.txt", "SimpleOS v0.3 - Virtual Memory Edition\n\nNew commands:\n  mem - show memory stats\n  vmm - show page mappings\n  alloc - test page allocation\n", 136);
    
    // Enable interrupts
    printf("[Init] Enabling interrupts...\n");
    enable_interrupts();
    
    // Print ready message
    printf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    printf("System ready! Type 'help' for commands.\n");
    printf("Try: mem, vmm, alloc\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    
    // Show initial prompt
    shell_prompt();

    // AUTO-TEST: Test process switching automatically
    // Uncomment to auto-start processes:
    // printf("\n[AUTO-TEST] Creating test processes...\n");
    // uint32_t pid1 = process_create("TestA", test_process_1, PRIORITY_REGULAR);
    // uint32_t pid2 = process_create("TestB", test_process_2, PRIORITY_REGULAR);
    // uint32_t pid3 = process_create("TestC", test_process_3, PRIORITY_REGULAR);
    // if (pid1 && pid2 && pid3) {
    //     printf("[AUTO-TEST] Created PIDs: %d, %d, %d\n", pid1, pid2, pid3);
    //     printf("[AUTO-TEST] Starting scheduler in 2 seconds...\n");
    //     for (volatile int i = 0; i < 50000000; i++) {}
    //     scheduler_start();
    // }

    // Main event loop
    while (1) {
        if (events_pending()) {
            input_event_t event = pop_event();
            
            switch (event.type) {
                case EVENT_TYPE_KEY_DOWN: {
                    char c = event.data[0];
                    
                    if (c == '\n') {
                        printf("\n");
                        cmd_buffer[cmd_pos] = '\0';
                        shell_process_command(cmd_buffer);
                        cmd_pos = 0;
                        shell_prompt();
                    }
                    else if (c == '\b') {
                        if (cmd_pos > 0) {
                            cmd_pos--;
                            vga_putchar('\b');
                        }
                    }
                    else if (c >= 32 && c < 127) {
                        if (cmd_pos < CMD_BUFFER_SIZE - 1) {
                            cmd_buffer[cmd_pos++] = c;
                            vga_putchar(c);
                        }
                    }
                    break;
                }
                
                case EVENT_TYPE_MOUSE_MOVE:
                    break;
                
                case EVENT_TYPE_MOUSE_CLICK: {
                    uint16_t x = *(uint16_t*)&event.data[0];
                    uint16_t y = *(uint16_t*)&event.data[2];
                    uint8_t button = event.data[4];
                    printf("\n[Mouse] Click at (%d, %d) button %d\n", x, y, button);
                    shell_prompt();
                    for (int i = 0; i < cmd_pos; i++) {
                        vga_putchar(cmd_buffer[i]);
                    }
                    break;
                }
            }
        }
        
        halt_cpu();
    }
}
