// File: exec.c
// Process execution and return handling
//
// run_process_and_wait() saves the kernel context with kctx_save()
// (a setjmp-style assembly routine in switch.asm), then switches to
// the process. When the process performs the EXIT syscall,
// exit_to_kernel() switches back to the kernel page directory and
// resumes the saved context with kctx_restore().
//
// The previous implementation used a GCC computed-goto label
// (&&return_point) plus an inline-asm jump. At -O2 the compiler
// placed the label at the top of the function and kept live values
// in callee-saved registers, so "returning" re-entered the setup
// code with a corrupted %ebx and ended up loading CR3 with 0,
// triple-faulting the machine. kctx_save/kctx_restore save and
// restore all callee-saved registers, ESP and EIP explicitly, which
// is safe at any optimization level.

#include "process.h"
#include "vmm.h"
#include "types.h"

extern void printf(const char* format, ...);
extern void process_set_current(process_t* proc);
extern void process_reap_terminated(void);
extern void vmm_switch_page_directory(uint32_t pd);
extern uint32_t vmm_get_current_directory(void);

// Assembly context helpers (switch.asm)
extern int  kctx_save(uint32_t ctx[6]);
extern void kctx_restore(uint32_t ctx[6]);

// Saved kernel context and page directory
static uint32_t kernel_ctx[6];
static uint32_t kernel_pd = 0;
static int      exec_active = 0;

// Called by the EXIT syscall to return to the kernel
void exit_to_kernel(void) {
    if (!exec_active) {
        printf("[Exec] WARNING: exit_to_kernel called with no active exec\n");
        return;
    }

    printf("[Exec] Returning to kernel...\n");

    // Switch back to the kernel page directory *before* touching
    // anything that may only be mapped there.
    vmm_switch_page_directory(kernel_pd);

    // Clear current process
    process_set_current(NULL);

    exec_active = 0;

    // Resume run_process_and_wait right after its kctx_save call.
    kctx_restore(kernel_ctx);

    // Should never reach here
    while (1) { __asm__ volatile("hlt"); }
}

// Assembly function to switch to process
extern void switch_to_process(process_t* proc);

// Run a process and wait for it to exit
void run_process_and_wait(process_t* proc) {
    if (!proc) return;

    // Save kernel page directory and execution context
    kernel_pd = vmm_get_current_directory();

    if (kctx_save(kernel_ctx) == 0) {
        // First pass: launch the process
        exec_active = 1;

        process_set_current(proc);
        proc->state = PROCESS_STATE_RUNNING;

        // Switch to process page directory
        vmm_switch_page_directory(proc->page_directory);

        printf("[Exec] Starting process '%s' (PID %d)\n", proc->name, proc->pid);

        // Jump to the process - will not return until EXIT syscall
        switch_to_process(proc);

        // Should never reach here
        printf("[Exec] ERROR: switch_to_process returned!\n");
        while (1) { __asm__ volatile("hlt"); }
    }

    // Second pass: resumed here by exit_to_kernel after the process
    // exited. We are back on the kernel page directory.
    //
    // The EXIT syscall arrived through an interrupt gate, so IF is
    // still clear at this point. Re-enable interrupts or the shell's
    // event loop will hlt forever with the timer and keyboard dead.
    __asm__ volatile("sti");

    printf("[Exec] Process completed normally\n");

    // Free the terminated process's page directory, kernel stack and
    // table slot (previously this leaked on every exec).
    process_reap_terminated();
}
