// File: sched64.c
// Preemptive round-robin scheduler for the 64-bit kernel (Phase B.5).
//
// Each task has a kernel stack holding a full interrupt frame. The
// timer IRQ stub (irq64_sched in isr64.asm) pushes all 15 GPRs, calls
// schedule64() with the stack pointer, and reloads the returned stack
// pointer before popping the GPRs and IRETQ - so a single mov swaps
// which task the CPU returns to.

#include <stdint.h>

extern void sputs(const char* s);
extern void sputdec(uint64_t v);
extern void sched_start_asm(uint64_t rsp) __attribute__((noreturn));
extern void tss_set_rsp0(uint64_t rsp0);

#define MAX_TASKS 8
#define KSTACK_SIZE 8192

// Full frame as laid out by irq64_sched: 15 GPRs (r15 lowest .. rax),
// then the CPU's IRETQ frame (rip, cs, rflags, rsp, ss).
struct frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t rip, cs, rflags, rsp, ss;
};

enum { TASK_UNUSED = 0, TASK_READY, TASK_RUNNING, TASK_DEAD };

typedef struct {
    uint64_t rsp;      // saved stack pointer (points at the frame)
    int state;
    char label;
} task_t;

static task_t tasks[MAX_TASKS];
static uint8_t kstacks[MAX_TASKS][KSTACK_SIZE] __attribute__((aligned(16)));
static int num_tasks;
static int current = -1;
static int live_tasks;

extern void outb(uint16_t, uint8_t);   // for EOI (provided below via inline)

static inline void pic_eoi(void) {
    __asm__ volatile("outb %0, $0x20" : : "a"((uint8_t)0x20));
}

// Create a ready task that starts at `entry` in ring 3 with the given
// user stack, and RBX/RBP preset (the task blob reads its character
// from RBX and iteration count from RBP).
int sched_add(uint64_t entry, uint64_t ustack, uint64_t rbx, uint64_t rbp,
              char label) {
    if (num_tasks >= MAX_TASKS) return -1;
    int i = num_tasks++;

    uint64_t top = (uint64_t)&kstacks[i][KSTACK_SIZE];
    struct frame* f = (struct frame*)(top - sizeof(struct frame));
    // zero all GPRs
    f->r15 = f->r14 = f->r13 = f->r12 = f->r11 = f->r10 = f->r9 = f->r8 = 0;
    f->rbp = rbp;
    f->rdi = f->rsi = f->rdx = f->rcx = 0;
    f->rbx = rbx;
    f->rax = 0;
    f->rip = entry;
    f->cs = 0x2B;              // 64-bit user code | RPL 3
    f->rflags = 0x202;         // IF=1
    f->rsp = ustack;
    f->ss = 0x23;              // user data | RPL 3

    tasks[i].rsp = (uint64_t)f;
    tasks[i].state = TASK_READY;
    tasks[i].label = label;
    live_tasks++;
    return i;
}

// Pick the next ready task after `from` (round robin).
static int next_ready(int from) {
    for (int n = 1; n <= num_tasks; n++) {
        int i = (from + n) % num_tasks;
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_RUNNING) {
            return i;
        }
    }
    return -1;
}

// Called from the timer IRQ stub with the current stack pointer.
// Returns the stack pointer to resume (possibly a different task).
uint64_t schedule64(uint64_t rsp) {
    pic_eoi();

    if (current >= 0 && tasks[current].state == TASK_RUNNING) {
        tasks[current].rsp = rsp;
        tasks[current].state = TASK_READY;
    }

    int next = next_ready(current < 0 ? 0 : current);
    if (next < 0) {
        // No runnable task: stay put (shouldn't happen while live > 0)
        return rsp;
    }
    current = next;
    tasks[current].state = TASK_RUNNING;
    // Point the ring0 stack at THIS task's kernel stack, so its next
    // preemption saves onto its own stack rather than a shared one.
    tss_set_rsp0((uint64_t)&kstacks[current][KSTACK_SIZE]);
    return tasks[current].rsp;
}

// Called from SYS_EXIT. Marks the running task dead and waits to be
// preempted away; when the last task exits, reports and halts.
void sched_task_exit(void) {
    if (current >= 0) {
        tasks[current].state = TASK_DEAD;
        sputs("\n[64] task '");
        char s[2] = { tasks[current].label, 0 };
        sputs(s);
        sputs("' exited.\n");
    }
    live_tasks--;

    if (live_tasks <= 0) {
        sputs("[64] All tasks finished. Preemptive multitasking works.\n");
        extern void sched_all_done(void);
        sched_all_done();
        for (;;) __asm__ volatile("cli; hlt");
    }

    // Let the timer preempt us to another task; this frame is abandoned.
    __asm__ volatile("sti");
    for (;;) __asm__ volatile("hlt");
}

char sched_current_label(void) {
    return current >= 0 ? tasks[current].label : '?';
}

// Launch the first task; never returns.
void sched_run(void) {
    sputs("[64] Scheduler: launching ");
    sputdec(num_tasks);
    sputs(" ring-3 tasks, round-robin at 100 Hz.\n");
    current = next_ready(0);
    tasks[current].state = TASK_RUNNING;
    tss_set_rsp0((uint64_t)&kstacks[current][KSTACK_SIZE]);
    sched_start_asm(tasks[current].rsp);
}
