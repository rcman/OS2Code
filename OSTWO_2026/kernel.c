// File: kernel.c
// Main kernel entry point - OS/Two v0.3 (OS/2-Compatible Operating System)

#include "types.h"
#include "vga.h"
#include "events.h"
#include "ramfs.h"
#include "pmm.h"
#include "vmm.h"
#include "process.h"
#include "gui.h"
#include "vga_gfx.h"
#include "vbe.h"
#include "bga.h"
#include "lx.h"
#include <stdarg.h>

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
extern void register_interrupt_handler(uint8_t n, isr_t handler);
extern void enter_user_mode(void);
extern void process_init(void);
extern void scheduler_init(void);
extern void semaphore_init(void);
extern void thread_init(void);
extern void scheduler_start(void);
extern void scheduler_schedule(void);
extern uint32_t process_create(const char* name, void (*entry_point)(void), uint32_t priority);
extern void process_print_table(void);
extern void test_process_1(void);
extern void test_process_2(void);
extern void test_process_3(void);
extern void test_process_4(void);
extern void test_syscall_validation(void);
extern void test_direct_io_gpf(void);
extern void test_phase2_process_apis(void);
extern void test_memory_apis(void);
extern void test_semaphore_apis(void);
extern void test_threading_apis(void);
extern void test_phase2_integration(void);
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
    uint16_t apm_table;
    uint16_t apm_table_size;
    uint32_t vbe_control_info;
    uint32_t vbe_mode_info;
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint64_t framebuffer_addr;
    uint32_t framebuffer_pitch;
    uint32_t framebuffer_width;
    uint32_t framebuffer_height;
    uint8_t framebuffer_bpp;
    uint8_t framebuffer_type;
    uint8_t color_info[6];
} __attribute__((packed)) multiboot_info_t;

// Simple shell
static void shell_prompt(void);
static void shell_process_command(const char* cmd);

// Command buffer
#define CMD_BUFFER_SIZE 256
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_pos = 0;

// Shell window (graphics mode)
static window_t* shell_window = 0;

// Forward printf output into the GUI shell window (installed once
// the window exists) so exec/ps/mem output and user-program writes
// are visible on screen, not just on the serial port.
extern void printf_set_gui_sink(void (*fn)(char));
static void shell_gui_sink(char c) {
    if (shell_window) {
        gui_window_putchar(shell_window, c);
    }
}

// Shell output helper
static void shell_print(const char* str) {
    if (shell_window) {
        gui_window_print(shell_window, str);
    } else {
        printf("%s", str);  // Fallback to VGA text mode
    }
}

// Shell printf - simplified version for shell commands
static void shell_printf(const char* format, ...) {
    char buffer[512];
    va_list args;
    va_start(args, format);

    // Simple vsnprintf replacement for common formats
    char* buf_ptr = buffer;
    const char* fmt_ptr = format;

    while (*fmt_ptr && (buf_ptr - buffer) < 510) {
        if (*fmt_ptr == '%') {
            fmt_ptr++;
            if (*fmt_ptr == 's') {
                // String
                const char* s = va_arg(args, const char*);
                while (*s && (buf_ptr - buffer) < 510) {
                    *buf_ptr++ = *s++;
                }
            } else if (*fmt_ptr == 'd') {
                // Decimal (simple version)
                int val = va_arg(args, int);
                if (val == 0) {
                    *buf_ptr++ = '0';
                } else {
                    char tmp[20];
                    int i = 0;
                    int neg = val < 0;
                    if (neg) val = -val;
                    while (val > 0) {
                        tmp[i++] = '0' + (val % 10);
                        val /= 10;
                    }
                    if (neg) *buf_ptr++ = '-';
                    while (i > 0) {
                        *buf_ptr++ = tmp[--i];
                    }
                }
            } else {
                *buf_ptr++ = '%';
                *buf_ptr++ = *fmt_ptr;
            }
            fmt_ptr++;
        } else {
            *buf_ptr++ = *fmt_ptr++;
        }
    }
    *buf_ptr = '\0';
    va_end(args);

    shell_print(buffer);
}

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
    shell_print("  ");
    shell_print(name);
    shell_print(" (");
    shell_printf("%d", size);
    shell_print(" bytes)\n");
}

// Shell prompt
static void shell_prompt(void) {
    if (shell_window) {
        shell_window->fg_color = 10;  // Light green
        shell_print("OS/Two> ");
        shell_window->fg_color = 15;  // White
        gui_draw_window(shell_window);  // Redraw to show prompt
    } else {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        printf("OS/Two> ");
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
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
        shell_print("Available commands:\n\n  System:\n");
        shell_print("    help              - Show this help\n");
        shell_print("    clear             - Clear screen\n");
        shell_print("    info              - Show system info\n");
        shell_print("    ls                - List all files\n");
        shell_print("    cat <file>        - Display file contents\n");
        shell_print("    reboot            - Reboot the system\n");
        shell_print("    halt              - Halt the CPU\n");
        shell_print("\nType command name for details\n");
    }
    else if (str_equal(cmd, "clear")) {
        if (shell_window) {
            gui_window_clear(shell_window);
        } else {
            vga_clear();
        }
    }
    else if (str_equal(cmd, "info")) {
        shell_print("OS/Two v0.3 (Virtual Memory)\n");
        shell_print("Architecture: i386 (32-bit)\n");
        shell_print("Timer frequency: 100 Hz\n");
        shell_printf("Current ticks: %d\n", timer_get_ticks());
        shell_printf("Files in RamFS: %d\n", ramfs_count());
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
    else if (str_starts_with(cmd, "exec ")) {
        // Execute a program from RamFS: exec <filename> [arguments...]
        char* filename = (char*)skip_whitespace(skip_to_whitespace(cmd));
        // Split arguments off the filename (they go to the program's
        // OS/2 command line and become argv[1..] in C programs)
        const char* exec_args = "";
        {
            char* p = filename;
            while (*p && *p != ' ' && *p != '\t') p++;
            if (*p) {
                *p = '\0';
                exec_args = skip_whitespace(p + 1);
            }
        }
        if (*filename == '\0') {
            printf("Usage: exec <filename>\n");
            printf("Available programs:\n");
            printf("  hello.bin - Prints a message and exits\n");
            printf("  loop.bin  - Prints X 10 times and exits\n");
            printf("  hello_os2.exe - OS/2 LX executable (DOSCALLS imports)\n");
        } else {
            printf("Executing '%s' from RamFS...\n", filename);

            // Get file size to verify it exists
            int file_size = ramfs_size(filename);
            if (file_size < 0) {
                printf("ERROR: File '%s' not found in RamFS\n", filename);
                printf("Use 'ls' to see available files.\n");
            } else {
                // Read binary from RamFS
                // Static: RAMFS_MAX_FILESIZE is far too large for the stack
                static uint8_t binary_buffer[RAMFS_MAX_FILESIZE];
                int bytes_read = ramfs_read(filename, binary_buffer, sizeof(binary_buffer));

                if (bytes_read != file_size) {
                    printf("ERROR: Failed to read file '%s'\n", filename);
                } else {
                    // Create process, picking the loader by file format:
                    // LX (OS/2 executable), ELF, or flat binary
                    uint32_t pid;
                    if (lx_validate(binary_buffer, file_size)) {
                        pid = lx_exec(filename, exec_args, binary_buffer, file_size);
                    } else if (file_size >= 4 && binary_buffer[0] == 0x7F &&
                               binary_buffer[1] == 'E' && binary_buffer[2] == 'L' &&
                               binary_buffer[3] == 'F') {
                        extern uint32_t elf_exec(const char* name, const void* data, uint32_t size);
                        pid = elf_exec(filename, binary_buffer, file_size);
                    } else {
                        pid = process_create_from_binary(filename, binary_buffer, file_size, PRIORITY_REGULAR);
                    }

                    if (pid == 0) {
                        printf("ERROR: Failed to create process from '%s'\n", filename);
                    } else {
                        printf("Running process %d...\n", pid);

                        // Get the process and run it directly (without scheduler)
                        process_t* proc = process_get(pid);
                        if (proc) {
                            extern void run_process_and_wait(process_t* proc);
                            run_process_and_wait(proc);
                            printf("\nProcess exited. Returning to shell.\n");
                        }
                    }
                }
            }
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
    else if (str_equal(cmd, "testsyscall")) {
        printf("Testing syscall interface...\n");
        printf("This process will test DosWrite, DosPutChar, and DosGetPID.\n");
        printf("It will run in user mode (Ring 3) with IOPL=0.\n\n");

        uint32_t pid = process_create("SyscallTest", test_syscall_validation, PRIORITY_REGULAR);
        uint32_t bg_pid = process_create("TestA", test_process_1, PRIORITY_REGULAR);

        if (pid && bg_pid) {
            printf("Created SyscallTest (PID %d) and background process (PID %d)\n\n", pid, bg_pid);
            scheduler_start();
        } else {
            printf("ERROR: Failed to create test processes!\n");
        }
    }
    else if (str_equal(cmd, "testdos")) {
        printf("=== Testing Phase 1 DOS API Functions ===\n\n");

        // Test DosGetDateTime
        printf("1. Testing DosGetDateTime...\n");
        extern void rtc_get_datetime(void* dt);
        typedef struct {
            uint8_t hours, minutes, seconds, hundredths;
            uint8_t day, month;
            uint16_t year;
            int16_t timezone;
            uint8_t weekday;
        } DateTime;
        DateTime dt;
        rtc_get_datetime(&dt);
        printf("   Current Date/Time: %d-%02d-%02d %02d:%02d:%02d\n",
               dt.year, dt.month, dt.day, dt.hours, dt.minutes, dt.seconds);
        const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
        if (dt.weekday < 7) {
            printf("   Day of week: %s\n", days[dt.weekday]);
        }

        // Test DosBeep
        printf("\n2. Testing DosBeep...\n");
        printf("   Playing 3 beeps at different frequencies...\n");
        extern void speaker_beep(uint32_t freq, uint32_t duration);
        speaker_beep(800, 200);   // 800 Hz for 200ms
        for (volatile int i = 0; i < 5000000; i++);  // Short delay
        speaker_beep(1000, 200);  // 1000 Hz for 200ms
        for (volatile int i = 0; i < 5000000; i++);
        speaker_beep(1200, 200);  // 1200 Hz for 200ms
        printf("   Beeps completed!\n");

        printf("\n=== All tests completed! ===\n");
    }
    else if (str_equal(cmd, "testproc2")) {
        printf("Testing Phase 2 Process APIs...\n");
        printf("Creating test process to demonstrate DosGetPPID, DosSetPriority\n\n");

        uint32_t pid = process_create("Phase2Test", test_phase2_process_apis, PRIORITY_REGULAR);

        if (pid) {
            printf("Created Phase2Test process (PID %d)\n", pid);

            // Get the process and run it
            process_t* proc = process_get(pid);
            if (proc) {
                extern void run_process_and_wait(process_t* proc);
                run_process_and_wait(proc);
                printf("\nTest process completed. Returned to shell.\n");
            }
        } else {
            printf("ERROR: Failed to create test process!\n");
        }
    }
    else if (str_equal(cmd, "testmem")) {
        printf("Testing Memory Management APIs...\n");
        printf("Creating test process to demonstrate DosAllocMem, DosFreeMem\n\n");

        uint32_t pid = process_create("MemTest", test_memory_apis, PRIORITY_REGULAR);

        if (pid) {
            printf("Created MemTest process (PID %d)\n", pid);

            // Get the process and run it
            process_t* proc = process_get(pid);
            if (proc) {
                extern void run_process_and_wait(process_t* proc);
                run_process_and_wait(proc);
                printf("\nTest process completed. Returned to shell.\n");
            }
        } else {
            printf("ERROR: Failed to create test process!\n");
        }
    }
    else if (str_equal(cmd, "testsem")) {
        printf("Testing Semaphore APIs...\n");
        printf("Creating test process to demonstrate DosCreateMutexSem, DosSemRequest, etc.\n\n");

        uint32_t pid = process_create("SemTest", test_semaphore_apis, PRIORITY_REGULAR);

        if (pid) {
            printf("Created SemTest process (PID %d)\n", pid);

            // Get the process and run it
            process_t* proc = process_get(pid);
            if (proc) {
                extern void run_process_and_wait(process_t* proc);
                run_process_and_wait(proc);
                printf("\nTest process completed. Returned to shell.\n");
            }
        } else {
            printf("ERROR: Failed to create test process!\n");
        }
    }
    else if (str_equal(cmd, "testthread")) {
        printf("Testing Threading APIs...\n");
        printf("Creating test process to demonstrate DosCreateThread, multi-threading\n\n");

        uint32_t pid = process_create("ThreadTest", test_threading_apis, PRIORITY_REGULAR);

        if (pid) {
            printf("Created ThreadTest process (PID %d)\n", pid);

            // Get the process and run it
            process_t* proc = process_get(pid);
            if (proc) {
                extern void run_process_and_wait(process_t* proc);
                run_process_and_wait(proc);
                printf("\nTest process completed. Returned to shell.\n");
            }
        } else {
            printf("ERROR: Failed to create test process!\n");
        }
    }
    else if (str_equal(cmd, "testphase2")) {
        printf("========================================\n");
        printf("  PHASE 2 COMPREHENSIVE TEST\n");
        printf("========================================\n");
        printf("This test exercises ALL Phase 2 features:\n");
        printf("  - Process Management APIs\n");
        printf("  - Memory Management APIs\n");
        printf("  - Synchronization APIs (Semaphores)\n");
        printf("  - Threading APIs\n\n");

        uint32_t pid = process_create("Phase2Test", test_phase2_integration, PRIORITY_REGULAR);

        if (pid) {
            printf("Created Phase2Test process (PID %d)\n\n", pid);

            // Get the process and run it
            process_t* proc = process_get(pid);
            if (proc) {
                extern void run_process_and_wait(process_t* proc);
                run_process_and_wait(proc);
                printf("\n========================================\n");
                printf("  Phase 2 test completed successfully!\n");
                printf("========================================\n\n");
            }
        } else {
            printf("ERROR: Failed to create test process!\n");
        }
    }
    else if (str_equal(cmd, "testgpf")) {
        printf("Testing IOPL=0 security...\n");
        printf("This process will attempt direct I/O from user mode.\n");
        printf("With IOPL=0, this SHOULD cause a General Protection Fault (GPF).\n");
        printf("If the kernel doesn't crash, GPF handling is working.\n\n");

        uint32_t pid = process_create("GPFTest", test_direct_io_gpf, PRIORITY_REGULAR);
        uint32_t bg_pid = process_create("TestA", test_process_1, PRIORITY_REGULAR);

        if (pid && bg_pid) {
            printf("Created GPFTest (PID %d) and background process (PID %d)\n\n", pid, bg_pid);
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
            shell_print("No files in filesystem.\n");
        } else {
            shell_print("Files in RamFS (");
            shell_printf("%d", count);
            shell_print("):\n");
            ramfs_list(list_file_callback);
        }
    }
    else if (str_equal(cmd, "df")) {
        shell_print("RamFS Filesystem Info:\n");
        shell_print("  Max files:     ");
        shell_printf("%d", RAMFS_MAX_FILES);
        shell_print("\n  Used files:    ");
        shell_printf("%d", ramfs_count());
        shell_print("\n  Free slots:    ");
        shell_printf("%d", RAMFS_MAX_FILES - ramfs_count());
        shell_print("\n  Max file size: ");
        shell_printf("%d", RAMFS_MAX_FILESIZE);
        shell_print(" bytes\n  Free space:    ");
        shell_printf("%d", ramfs_free_space());
        shell_print(" bytes\n");
    }
    else if (str_starts_with(cmd, "touch ")) {
        const char* name = skip_whitespace(cmd + 6);
        if (*name == '\0') {
            shell_print("Usage: touch <filename>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* end = skip_to_whitespace(name);
            int len = end - name;
            if (len >= RAMFS_MAX_FILENAME) len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, name, len + 1);

            if (ramfs_exists(filename)) {
                shell_print("File '");
                shell_print(filename);
                shell_print("' already exists.\n");
            } else if (ramfs_create(filename) == 0) {
                shell_print("Created file '");
                shell_print(filename);
                shell_print("'\n");
            } else {
                shell_print("Error: Could not create file.\n");
            }
        }
    }
    else if (str_starts_with(cmd, "rm ")) {
        const char* name = skip_whitespace(cmd + 3);
        if (*name == '\0') {
            shell_print("Usage: rm <filename>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* end = skip_to_whitespace(name);
            int len = end - name;
            if (len >= RAMFS_MAX_FILENAME) len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, name, len + 1);

            if (ramfs_delete(filename) == 0) {
                shell_print("Deleted '");
                shell_print(filename);
                shell_print("'\n");
            } else {
                shell_print("Error: File '");
                shell_print(filename);
                shell_print("' not found.\n");
            }
        }
    }
    else if (str_starts_with(cmd, "cat ")) {
        const char* name = skip_whitespace(cmd + 4);
        if (*name == '\0') {
            shell_print("Usage: cat <filename>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* end = skip_to_whitespace(name);
            int len = end - name;
            if (len >= RAMFS_MAX_FILENAME) len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, name, len + 1);

            if (!ramfs_exists(filename)) {
                shell_print("Error: File '");
                shell_print(filename);
                shell_print("' not found.\n");
            } else {
                char buffer[RAMFS_MAX_FILESIZE + 1];
                int bytes = ramfs_read(filename, buffer, RAMFS_MAX_FILESIZE);
                if (bytes >= 0) {
                    buffer[bytes] = '\0';
                    if (bytes == 0) {
                        shell_print("(empty file)\n");
                    } else {
                        shell_print(buffer);
                        if (bytes > 0 && buffer[bytes-1] != '\n') {
                            shell_print("\n");
                        }
                    }
                } else {
                    shell_print("Error reading file.\n");
                }
            }
        }
    }
    else if (str_starts_with(cmd, "write ")) {
        const char* args = skip_whitespace(cmd + 6);
        if (*args == '\0') {
            shell_print("Usage: write <filename> <text>\n");
        } else {
            char filename[RAMFS_MAX_FILENAME];
            const char* name_end = skip_to_whitespace(args);
            int name_len = name_end - args;
            if (name_len >= RAMFS_MAX_FILENAME) name_len = RAMFS_MAX_FILENAME - 1;
            str_copy(filename, args, name_len + 1);

            const char* content = skip_whitespace(name_end);
            if (*content == '\0') {
                shell_print("Usage: write <filename> <text>\n");
            } else {
                int content_len = str_len(content);
                int written = ramfs_write(filename, content, content_len);
                if (written >= 0) {
                    shell_print("Wrote ");
                    shell_printf("%d", written);
                    shell_print(" bytes to '");
                    shell_print(filename);
                    shell_print("'\n");
                } else {
                    shell_print("Error: Could not write to file.\n");
                }
            }
        }
    }
    else {
        shell_print("Unknown command: ");
        shell_print(cmd);
        shell_print("\nType 'help' for available commands.\n");
    }
}

// Main kernel entry point
void kmain(uint32_t magic, multiboot_info_t* mbi) {
    // Initialize VGA text mode
    vga_init();
    
    // Print banner
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    printf("=====================================\n");
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    printf("     OS/Two v0.3 (Alpha)\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    printf("  OS/2-Compatible Operating System\n");
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
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

    // Initialize VBE framebuffer (if available from bootloader)
    printf("[Init] Checking for VBE framebuffer...\n");
    vbe_init_from_multiboot(mbi->flags, (void*)&mbi->framebuffer_addr);
    vbe_mode_info_t* vbe_info = vbe_get_mode_info();
    if (vbe_info && vbe_info->enabled) {
        printf("[VBE] Framebuffer detected: %dx%dx%d at 0x%x\n",
               vbe_info->width, vbe_info->height, vbe_info->bpp, vbe_info->framebuffer);

        // Map framebuffer into virtual memory (must happen AFTER vmm_init)
        // We'll do this after VMM is initialized
    } else {
        // No framebuffer from the bootloader (e.g. QEMU -kernel boot).
        // Try the Bochs/QEMU display adapter, which can be programmed
        // to a high-resolution mode directly from protected mode.
        printf("[VBE] No bootloader framebuffer, probing Bochs/QEMU adapter...\n");
        if (bga_init(1024, 768, 32) == 0) {
            printf("[VBE] High-resolution framebuffer ready: %dx%dx%d at 0x%x\n",
                   vbe_info->width, vbe_info->height, vbe_info->bpp, vbe_info->framebuffer);
        } else {
            printf("[VBE] No framebuffer available, will use legacy VGA\n");
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

    // Map VBE framebuffer if detected
    if (vbe_info && vbe_info->enabled) {
        printf("[VBE] Mapping framebuffer into virtual memory...\n");
        uint32_t fb_size = vbe_info->height * vbe_info->pitch;
        uint32_t num_pages = (fb_size + 4095) / 4096;  // Round up to pages
        uint32_t fb_phys = vbe_info->framebuffer;

        for (uint32_t i = 0; i < num_pages; i++) {
            uint32_t virt = fb_phys + (i * 4096);
            uint32_t phys = fb_phys + (i * 4096);
            if (!vmm_map_page(virt, phys, PTE_WRITABLE)) {
                printf("[VBE] ERROR: Failed to map framebuffer page %d\n", i);
                vbe_info->enabled = 0;  // Disable VBE on failure
                break;
            }
        }

        if (vbe_info->enabled) {
            printf("[VBE] Mapped %d pages (%d MB) at 0x%x\n",
                   num_pages, (num_pages * 4) / 1024, fb_phys);
        }
    }

    // Initialize timer (100 Hz)
    timer_init(100);
    
    // Initialize keyboard
    keyboard_init();
    
    // Initialize mouse
    mouse_init();
    
    // Initialize syscalls
    syscall_init();

    // Initialize PC speaker
    extern void speaker_init(void);
    speaker_init();

    // Initialize Real-Time Clock
    extern void rtc_init(void);
    rtc_init();

    // Initialize graphics (stays in text mode)
    graphics_init();
    
    // Initialize Virtual File System
    printf("[Init] Initializing VFS...\n");
    extern void vfs_init(void);
    extern void ramfs_vfs_init(void);
    extern void dos_file_init(void);
    extern void linux_syscall_init(void);
    extern void serial_init(uint16_t port, uint16_t baud);
    extern void parallel_init(uint16_t port);
    extern void ide_init(void);
    extern void vga_gfx_init(void);
    extern void ipc_init(void);
    vfs_init();
    ramfs_vfs_init();
    dos_file_init();
    linux_syscall_init();

    // Initialize device drivers
    printf("[Init] Initializing device drivers...\n");
    serial_init(0x3F8, 12);  // COM1 at 9600 baud
    parallel_init(0x378);     // LPT1
    ide_init();               // IDE hard disks
    vga_gfx_init();           // VGA graphics subsystem

    // Initialize IPC subsystem
    printf("[Init] Initializing IPC subsystem...\n");
    ipc_init();

    // Initialize PCI and VirtIO
    printf("[Init] Initializing PCI and VirtIO...\n");
    extern void pci_init(void);
    extern void virtio_init(void);
    pci_init();
    virtio_init();

    // Initialize networking stack
    printf("[Init] Initializing network stack...\n");
    extern void net_init(void);
    extern void virtio_net_init(void);
    net_init();
    virtio_net_init();

    // Keep old RamFS for backward compatibility
    printf("[Init] Initializing legacy RamFS...\n");
    ramfs_init();
    printf("[RamFS] Initialized (%d file slots, %d bytes/file)\n",
           RAMFS_MAX_FILES, RAMFS_MAX_FILESIZE);

    // Load built-in user programs into RamFS
    #include "hello_bin.h"
    #include "loop_bin.h"
    #include "hello_os2_bin.h"
    #include "hello2_bin.h"
    #include "hello3_bin.h"
    #include "packed_bin.h"
    #include "hello4_bin.h"
    #include "hello5_bin.h"

    printf("[Debug] hello_bin address: 0x%x, length: %d\n", (uint32_t)hello_bin, hello_bin_len);
    printf("[Debug] loop_bin address: 0x%x, length: %d\n", (uint32_t)loop_bin, loop_bin_len);
    printf("[Debug] First bytes of hello_bin: %02x %02x %02x %02x\n",
           hello_bin[0], hello_bin[1], hello_bin[2], hello_bin[3]);

    int r1 = ramfs_write("hello.bin", hello_bin, hello_bin_len);
    int r2 = ramfs_write("loop.bin", loop_bin, loop_bin_len);
    int r3 = ramfs_write("hello_os2.exe", hello_os2_bin, hello_os2_bin_len);
    int r4 = ramfs_write("hello2.exe", hello2_bin, hello2_bin_len);
    int r5 = ramfs_write("hello3.exe", hello3_bin, hello3_bin_len);
    int r6 = ramfs_write("packed.exe", packed_bin, packed_bin_len);
    int r7 = ramfs_write("hello4.exe", hello4_bin, hello4_bin_len);
    int r8 = ramfs_write("hello5.exe", hello5_bin, hello5_bin_len);

    printf("[Debug] ramfs_count after write: %d\n", ramfs_count());

    if (r1 > 0 && r2 > 0 && r3 > 0 && r4 > 0 && r5 > 0 && r6 > 0 && r7 > 0 && r8 > 0) {
        printf("[RamFS] Loaded 8 built-in programs (incl. 6 OS/2 LX executables)\n");
    } else {
        printf("[RamFS] ERROR: Failed to load programs (%d, %d, %d, %d, %d, %d, %d, %d)\n",
               r1, r2, r3, r4, r5, r6, r7, r8);
    }

    // Initialize process management
    printf("[Init] Initializing process manager...\n");
    process_init();

    // Initialize semaphores
    printf("[Init] Initializing semaphores...\n");
    semaphore_init();

    // Initialize threads
    printf("[Init] Initializing threads...\n");
    thread_init();

    // Initialize scheduler
    printf("[Init] Initializing scheduler...\n");
    scheduler_init();
    
    // Create welcome files
    ramfs_write("welcome.txt", "Welcome to OS/Two v0.3!\nOS/2-Compatible Operating System\n", 58);
    ramfs_write("readme.txt", "OS/Two v0.3 - OS/2 API Compatible\n\nDOS API Functions:\n  DosWrite, DosExit, DosGetPID\n\nCommands:\n  mem - memory stats\n  vmm - page mappings\n  ps - process list\n", 136);
    
    // Enable interrupts
    printf("[Init] Enabling interrupts...\n");
    enable_interrupts();

    // Print ready message (in text mode)
    printf("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    printf("System ready! Type 'help' for commands.\n");
    printf("Try: mem, vmm, alloc\n\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // Show files in RamFS
    printf("Built-in programs:\n");
    int boot_count = ramfs_count();
    if (boot_count > 0) {
        ramfs_list(list_file_callback);
    }
    printf("\n");

    // Switch to graphics mode and initialize GUI
    printf("[Init] Switching to graphics mode...\n");
    vga_set_mode(0x13);  // Will use VBE if available, else Mode 13h

    printf("[Init] Initializing GUI...\n");
    gui_init();

    // Create shell window (use 90% of screen)
    printf("[Init] Creating shell window...\n");
    vga_mode_info_t* mode_info = vga_get_mode_info();
    int win_width = (mode_info->width * 9) / 10;   // 90% of screen width
    int win_height = (mode_info->height * 9) / 10; // 90% of screen height
    int win_x = (mode_info->width - win_width) / 2;
    int win_y = (mode_info->height - win_height) / 2;
    shell_window = gui_create_window(win_x, win_y, win_width, win_height, "OS/Two Shell");

    if (shell_window) {
        // Route kernel printf output into this window as well
        printf_set_gui_sink(shell_gui_sink);

        // Print welcome message to shell window
        shell_print("OS/Two v0.3 - Graphics Mode\n");
        shell_print("OS/2-Compatible Operating System\n\n");
        shell_print("System ready! Type 'help' for commands.\n");
        shell_print("Try: mem, vmm, alloc, info\n\n");

        // Show built-in programs
        char count_str[32];
        count_str[0] = '0' + (boot_count % 10);
        count_str[1] = '\0';
        shell_print("Built-in programs: ");
        shell_print(count_str);
        shell_print(" files\n\n");
    }

    // AUTO-TEST: Disabled - use 'exec hello.bin' command instead
    // printf("=== AUTO-TEST: Executing hello.bin ===\n");
    // uint8_t test_buffer[4096];
    // int test_size = ramfs_read("hello.bin", test_buffer, sizeof(test_buffer));
    // if (test_size > 0) {
    //     uint32_t test_pid = process_create_from_binary("hello.bin", test_buffer, test_size, PRIORITY_REGULAR);
    //     if (test_pid > 0) {
    //         printf("Created test process PID %d\n", test_pid);
    //         scheduler_start();
    //         printf("Scheduler started, calling scheduler_schedule()...\n");
    //         scheduler_schedule();
    //         printf("Back from scheduler_schedule (should never see this)\n");
    //     }
    // }

    // Show initial prompt
    shell_prompt();

    // Main event loop
    while (1) {
        if (events_pending()) {
            input_event_t event = pop_event();

            switch (event.type) {
                case EVENT_TYPE_KEY_DOWN: {
                    char c = event.data[0];

                    if (c == '\n') {
                        if (shell_window) {
                            gui_window_putchar(shell_window, '\n');
                        } else {
                            printf("\n");
                        }
                        cmd_buffer[cmd_pos] = '\0';
                        shell_process_command(cmd_buffer);
                        cmd_pos = 0;
                        shell_prompt();
                    }
                    else if (c == '\b') {
                        if (cmd_pos > 0) {
                            cmd_pos--;
                            if (shell_window) {
                                gui_window_putchar(shell_window, '\b');
                                gui_draw_window(shell_window);
                            } else {
                                vga_putchar('\b');
                            }
                        }
                    }
                    else if (c >= 32 && c < 127) {
                        if (cmd_pos < CMD_BUFFER_SIZE - 1) {
                            cmd_buffer[cmd_pos++] = c;
                            if (shell_window) {
                                gui_window_putchar(shell_window, c);
                                gui_draw_window(shell_window);
                            } else {
                                vga_putchar(c);
                            }
                        }
                    }
                    break;
                }

                case EVENT_TYPE_MOUSE_MOVE: {
                    if (shell_window) {
                        uint16_t x = *(uint16_t*)&event.data[0];
                        uint16_t y = *(uint16_t*)&event.data[2];
                        gui_handle_mouse_move(x, y);
                    }
                    break;
                }

                case EVENT_TYPE_MOUSE_CLICK: {
                    uint16_t x = *(uint16_t*)&event.data[0];
                    uint16_t y = *(uint16_t*)&event.data[2];
                    uint8_t button = event.data[4];

                    if (shell_window) {
                        // Handle mouse down
                        gui_handle_mouse_down(x, y, button);
                    } else {
                        printf("\n[Mouse] Click at (%d, %d) button %d\n", x, y, button);
                        shell_prompt();
                        for (int i = 0; i < cmd_pos; i++) {
                            vga_putchar(cmd_buffer[i]);
                        }
                    }
                    break;
                }

                case EVENT_TYPE_MOUSE_RELEASE: {
                    uint16_t x = *(uint16_t*)&event.data[0];
                    uint16_t y = *(uint16_t*)&event.data[2];
                    uint8_t button = event.data[4];

                    if (shell_window) {
                        // Handle mouse up
                        gui_handle_mouse_up(x, y, button);
                    }
                    break;
                }
            }
        }

        halt_cpu();
    }
}
