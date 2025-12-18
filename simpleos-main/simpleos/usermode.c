// File: usermode.c
// User mode support and testing

#include "types.h"
#include "vga.h"

// External printf
extern void printf(const char* format, ...);

// External TSS function
extern void tss_set_kernel_stack(uint32_t stack);

// User mode test function - will be executed in Ring 3
static void user_mode_test_function(void) {
    // This code runs in Ring 3 (user mode)
    // We can't directly access kernel functions from here

    // Try to make a system call back to kernel mode
    __asm__ volatile(
        "mov $0x01, %%eax\n"  // System call number 1 (test)
        "int $0x80\n"          // Trigger syscall interrupt
        : : : "eax"
    );

    // Loop forever (user mode has no way to return)
    while (1) {
        __asm__ volatile("hlt");
    }
}

// Jump to user mode
void enter_user_mode(void) {
    printf("[UserMode] Preparing to enter Ring 3...\n");

    // Set up a user mode stack
    // We'll use a simple stack at 0x800000 (8MB mark)
    uint32_t user_stack = 0x800000;

    // Allocate and map the user stack page
    extern bool vmm_alloc_page(uint32_t virt, uint32_t flags);
    extern uint32_t vmm_get_physical(uint32_t virt);

    // Map user stack (writable, user accessible)
    #define PTE_WRITABLE (1 << 1)
    #define PTE_USER     (1 << 2)

    if (!vmm_alloc_page(user_stack - 4096, PTE_WRITABLE | PTE_USER)) {
        printf("[UserMode] ERROR: Failed to allocate user stack!\n");
        return;
    }

    printf("[UserMode] User stack allocated at 0x%x\n", user_stack);
    printf("[UserMode] Entry point at 0x%x\n", (uint32_t)user_mode_test_function);

    // Map the code page as user-executable
    uint32_t code_page = ((uint32_t)user_mode_test_function) & 0xFFFFF000;
    uint32_t code_phys = vmm_get_physical(code_page);

    if (code_phys == 0) {
        printf("[UserMode] ERROR: Code page not mapped!\n");
        return;
    }

    // Make code page user-accessible
    extern bool vmm_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
    #define PTE_PRESENT  (1 << 0)
    vmm_map_page(code_page, code_phys, PTE_PRESENT | PTE_USER);

    printf("[UserMode] Code page mapped with user permissions\n");
    printf("[UserMode] Jumping to Ring 3...\n\n");

    // Prepare to jump to user mode
    // We need to:
    // 1. Set up the stack frame for iret
    // 2. Use iret to switch to Ring 3

    __asm__ volatile(
        "cli\n"                      // Disable interrupts during transition
        "mov $0x23, %%ax\n"         // User data selector (0x20 | RPL 3)
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"

        // Push values for iret
        "pushl $0x23\n"             // SS (user data selector)
        "pushl %0\n"                // ESP (user stack)
        "pushf\n"                   // EFLAGS
        "popl %%eax\n"
        "orl $0x200, %%eax\n"       // Set IF (interrupt flag)
        "pushl %%eax\n"             // Push modified EFLAGS
        "pushl $0x1B\n"             // CS (user code selector: 0x18 | RPL 3)
        "pushl %1\n"                // EIP (entry point)

        "iret\n"                     // Jump to Ring 3!
        : : "r"(user_stack), "r"((uint32_t)user_mode_test_function)
    );

    // Should never reach here
    printf("[UserMode] ERROR: Returned from user mode!\n");
}

// Test system call handler
void usermode_syscall_handler(registers_t* regs) {
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    printf("\n[Syscall] SUCCESS! System call from Ring 3 received!\n");
    printf("[Syscall] EAX (syscall number) = 0x%x\n", regs->eax);
    printf("[Syscall] EIP (user code) = 0x%x\n", regs->eip);
    printf("[Syscall] CS = 0x%x (Ring %d)\n", regs->cs, regs->cs & 0x3);
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    // For now, halt after successful syscall
    printf("\n[Syscall] User mode test PASSED!\n");
    printf("[Syscall] Halting system...\n");

    extern void disable_interrupts(void);
    extern void halt_cpu(void);
    disable_interrupts();
    while (1) halt_cpu();
}
