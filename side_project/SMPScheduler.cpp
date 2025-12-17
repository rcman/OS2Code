/*
 * OSFree SMP Scheduler - Modern Multicore Support for OS/2 Compatibility
 * 
 * This scheduler provides:
 * - Per-CPU run queues for true parallel execution
 * - OS/2 priority class compatibility (Idle, Regular, Server, Time-Critical)
 * - Work stealing for load balancing
 * - CPU affinity support
 * - NUMA awareness
 * - Lock-free operations where possible
 */

#include <stdint.h>
#include <stdatomic.h>
#include <stdbool.h>

/* OS/2 Priority Classes - Must maintain compatibility */
#define PRTYC_NOCHANGE      0
#define PRTYC_IDLETIME      1
#define PRTYC_REGULAR       2
#define PRTYC_TIMECRITICAL  3
#define PRTYC_FOREGROUNDSERVER 4

/* Priority levels within each class (0-31 delta) */
#define PRTYD_MINIMUM      -31
#define PRTYD_MAXIMUM       31

/* Scheduler constants */
#define MAX_CPUS           256
#define MAX_PRIORITY       127  /* 4 classes * 32 levels */
#define TIME_SLICE_MS      10   /* Default timeslice */
#define LOAD_BALANCE_MS    50   /* Load balance interval */

/* Thread states */
typedef enum {
    THREAD_STATE_READY,
    THREAD_STATE_RUNNING,
    THREAD_STATE_BLOCKED,
    THREAD_STATE_SUSPENDED,
    THREAD_STATE_TERMINATED
} thread_state_t;

/* Per-thread structure */
typedef struct thread {
    uint32_t tid;                    /* Thread ID */
    uint32_t pid;                    /* Process ID */
    
    /* OS/2 Priority Information */
    uint8_t priority_class;          /* PRTYC_* value */
    int8_t priority_delta;           /* -31 to +31 */
    uint8_t effective_priority;      /* Computed priority (0-127) */
    
    /* Scheduling state */
    thread_state_t state;
    uint32_t cpu_id;                 /* Currently assigned CPU */
    atomic_uint_fast32_t cpu_affinity_mask;  /* Bitmask of allowed CPUs */
    
    /* Time accounting */
    uint64_t time_slice_remaining;   /* Nanoseconds */
    uint64_t total_runtime;          /* Total CPU time used */
    uint64_t last_scheduled;         /* TSC when last scheduled */
    
    /* Queue links */
    struct thread *next;
    struct thread *prev;
    
    /* NUMA optimization */
    uint32_t numa_node;              /* Preferred NUMA node */
    
    /* OS/2 specific flags */
    bool is_16bit;                   /* 16-bit OS/2 app */
    bool is_dos;                     /* DOS session */
    bool is_win16;                   /* Win16 session */
    
    /* Context pointer (CPU-specific) */
    void *context;
} thread_t;

/* Per-CPU run queue structure */
typedef struct cpu_runqueue {
    uint32_t cpu_id;
    atomic_bool lock;                /* Spinlock for queue operations */
    
    /* Priority queues - one per priority level */
    thread_t *queues[MAX_PRIORITY + 1];
    atomic_uint_fast32_t queue_bitmap[4];  /* Bitmask for non-empty queues */
    
    /* Currently running thread */
    thread_t *current;
    
    /* Load balancing */
    atomic_uint_fast32_t num_threads;
    atomic_uint_fast64_t load;       /* Running average load */
    
    /* Statistics */
    uint64_t total_switches;
    uint64_t idle_time;
    
    /* NUMA node this CPU belongs to */
    uint32_t numa_node;
} cpu_runqueue_t;

/* Global scheduler state */
typedef struct scheduler {
    cpu_runqueue_t runqueues[MAX_CPUS];
    atomic_uint_fast32_t num_cpus;
    atomic_bool initialized;
    
    /* Global thread list (for management) */
    atomic_uint_fast32_t next_tid;
    
    /* NUMA topology */
    uint32_t num_numa_nodes;
    uint32_t cpus_per_node[MAX_CPUS / 8];  /* CPUs in each NUMA node */
} scheduler_t;

static scheduler_t g_scheduler = {0};

/* ============================================
 * Priority Calculation - OS/2 Compatible
 * ============================================ */
static inline uint8_t calculate_priority(uint8_t class, int8_t delta) {
    /* OS/2 priority mapping:
     * Idle Time:      0-31
     * Regular:        32-63
     * Server:         64-95
     * Time Critical:  96-127
     */
    uint8_t base_priority = 0;
    
    switch (class) {
        case PRTYC_IDLETIME:
            base_priority = 0;
            break;
        case PRTYC_REGULAR:
            base_priority = 32;
            break;
        case PRTYC_FOREGROUNDSERVER:
            base_priority = 64;
            break;
        case PRTYC_TIMECRITICAL:
            base_priority = 96;
            break;
        default:
            base_priority = 32;  /* Default to regular */
    }
    
    /* Apply delta, clamping to valid range within class */
    int priority = base_priority + 16 + delta;  /* 16 = middle of range */
    
    /* Clamp to class boundaries */
    int class_min = base_priority;
    int class_max = base_priority + 31;
    
    if (priority < class_min) priority = class_min;
    if (priority > class_max) priority = class_max;
    
    return (uint8_t)priority;
}

/* ============================================
 * CPU Affinity Management
 * ============================================ */
static inline bool cpu_in_affinity(thread_t *thread, uint32_t cpu_id) {
    uint32_t mask = atomic_load(&thread->cpu_affinity_mask);
    return (mask & (1U << cpu_id)) != 0;
}

static inline uint32_t find_best_cpu(thread_t *thread) {
    uint32_t affinity = atomic_load(&thread->cpu_affinity_mask);
    uint32_t num_cpus = atomic_load(&g_scheduler.num_cpus);
    uint32_t best_cpu = 0;
    uint32_t min_load = UINT32_MAX;
    
    /* Prefer CPUs in same NUMA node */
    for (uint32_t i = 0; i < num_cpus; i++) {
        if (!(affinity & (1U << i))) continue;
        
        cpu_runqueue_t *rq = &g_scheduler.runqueues[i];
        uint32_t load = atomic_load(&rq->num_threads);
        
        /* Bonus for same NUMA node */
        if (rq->numa_node == thread->numa_node) {
            load = (load * 3) / 4;  /* 25% bonus */
        }
        
        if (load < min_load) {
            min_load = load;
            best_cpu = i;
        }
    }
    
    return best_cpu;
}

/* ============================================
 * Queue Operations (Lock-Free Where Possible)
 * ============================================ */
static void acquire_runqueue_lock(cpu_runqueue_t *rq) {
    bool expected = false;
    while (!atomic_compare_exchange_weak(&rq->lock, &expected, true)) {
        expected = false;
        /* Spin with pause instruction equivalent */
        __asm__ volatile("pause" ::: "memory");
    }
}

static void release_runqueue_lock(cpu_runqueue_t *rq) {
    atomic_store(&rq->lock, false);
}

static void enqueue_thread(cpu_runqueue_t *rq, thread_t *thread) {
    acquire_runqueue_lock(rq);
    
    uint8_t prio = thread->effective_priority;
    
    /* Add to priority queue */
    thread->next = rq->queues[prio];
    thread->prev = NULL;
    
    if (rq->queues[prio]) {
        rq->queues[prio]->prev = thread;
    }
    
    rq->queues[prio] = thread;
    
    /* Update bitmap */
    uint32_t word = prio / 32;
    uint32_t bit = prio % 32;
    atomic_fetch_or(&rq->queue_bitmap[word], 1U << bit);
    
    atomic_fetch_add(&rq->num_threads, 1);
    thread->state = THREAD_STATE_READY;
    
    release_runqueue_lock(rq);
}

static thread_t *dequeue_highest_priority(cpu_runqueue_t *rq) {
    acquire_runqueue_lock(rq);
    
    thread_t *thread = NULL;
    
    /* Find highest priority non-empty queue */
    for (int word = 3; word >= 0; word--) {
        uint32_t bitmap = atomic_load(&rq->queue_bitmap[word]);
        if (bitmap == 0) continue;
        
        /* Find highest set bit (highest priority) */
        int bit = 31;
        while (bit >= 0) {
            if (bitmap & (1U << bit)) {
                uint8_t prio = word * 32 + bit;
                thread = rq->queues[prio];
                
                if (thread) {
                    /* Remove from queue */
                    rq->queues[prio] = thread->next;
                    if (thread->next) {
                        thread->next->prev = NULL;
                    }
                    
                    /* Update bitmap if queue empty */
                    if (!rq->queues[prio]) {
                        atomic_fetch_and(&rq->queue_bitmap[word], ~(1U << bit));
                    }
                    
                    atomic_fetch_sub(&rq->num_threads, 1);
                    thread->next = thread->prev = NULL;
                    break;
                }
            }
            bit--;
        }
        
        if (thread) break;
    }
    
    release_runqueue_lock(rq);
    return thread;
}

/* ============================================
 * Core Scheduler Functions
 * ============================================ */

/* Initialize scheduler subsystem */
void sched_init(uint32_t num_cpus, uint32_t *numa_topology) {
    if (atomic_load(&g_scheduler.initialized)) return;
    
    atomic_store(&g_scheduler.num_cpus, num_cpus);
    
    for (uint32_t i = 0; i < num_cpus; i++) {
        cpu_runqueue_t *rq = &g_scheduler.runqueues[i];
        rq->cpu_id = i;
        atomic_store(&rq->lock, false);
        atomic_store(&rq->num_threads, 0);
        atomic_store(&rq->load, 0);
        rq->numa_node = numa_topology ? numa_topology[i] : 0;
        rq->current = NULL;
        
        for (int j = 0; j <= MAX_PRIORITY; j++) {
            rq->queues[j] = NULL;
        }
        
        for (int j = 0; j < 4; j++) {
            atomic_store(&rq->queue_bitmap[j], 0);
        }
    }
    
    atomic_store(&g_scheduler.next_tid, 1);
    atomic_store(&g_scheduler.initialized, true);
}

/* Create new thread */
thread_t *sched_create_thread(uint32_t pid, uint8_t priority_class, 
                               int8_t priority_delta, uint32_t affinity_mask) {
    thread_t *thread = (thread_t *)malloc(sizeof(thread_t));
    if (!thread) return NULL;
    
    thread->tid = atomic_fetch_add(&g_scheduler.next_tid, 1);
    thread->pid = pid;
    thread->priority_class = priority_class;
    thread->priority_delta = priority_delta;
    thread->effective_priority = calculate_priority(priority_class, priority_delta);
    thread->state = THREAD_STATE_READY;
    
    /* Set affinity - default to all CPUs if not specified */
    uint32_t num_cpus = atomic_load(&g_scheduler.num_cpus);
    if (affinity_mask == 0) {
        affinity_mask = (1U << num_cpus) - 1;  /* All CPUs */
    }
    atomic_store(&thread->cpu_affinity_mask, affinity_mask);
    
    /* Find best CPU and NUMA node */
    thread->cpu_id = find_best_cpu(thread);
    thread->numa_node = g_scheduler.runqueues[thread->cpu_id].numa_node;
    
    /* Initialize time accounting */
    thread->time_slice_remaining = TIME_SLICE_MS * 1000000ULL;  /* Convert to ns */
    thread->total_runtime = 0;
    thread->last_scheduled = 0;
    
    thread->next = thread->prev = NULL;
    thread->is_16bit = false;
    thread->is_dos = false;
    thread->is_win16 = false;
    thread->context = NULL;
    
    /* Add to run queue */
    enqueue_thread(&g_scheduler.runqueues[thread->cpu_id], thread);
    
    return thread;
}

/* Set thread priority - OS/2 DosSetPriority compatible */
int sched_set_priority(thread_t *thread, uint8_t priority_class, int8_t priority_delta) {
    if (!thread) return -1;
    
    if (priority_class != PRTYC_NOCHANGE) {
        thread->priority_class = priority_class;
    }
    
    thread->priority_delta = priority_delta;
    thread->effective_priority = calculate_priority(thread->priority_class, 
                                                     thread->priority_delta);
    
    /* If thread is ready, re-enqueue with new priority */
    if (thread->state == THREAD_STATE_READY) {
        cpu_runqueue_t *rq = &g_scheduler.runqueues[thread->cpu_id];
        /* Remove and re-add (simplified - real impl would be more efficient) */
        enqueue_thread(rq, thread);
    }
    
    return 0;
}

/* Schedule next thread on current CPU */
thread_t *sched_schedule(uint32_t cpu_id) {
    cpu_runqueue_t *rq = &g_scheduler.runqueues[cpu_id];
    
    /* Save current thread if any */
    if (rq->current && rq->current->state == THREAD_STATE_RUNNING) {
        /* Thread's timeslice expired or it yielded */
        rq->current->state = THREAD_STATE_READY;
        rq->current->time_slice_remaining = TIME_SLICE_MS * 1000000ULL;
        enqueue_thread(rq, rq->current);
    }
    
    /* Pick highest priority ready thread */
    thread_t *next = dequeue_highest_priority(rq);
    
    if (next) {
        next->state = THREAD_STATE_RUNNING;
        next->cpu_id = cpu_id;
        next->last_scheduled = __rdtsc();  /* Read CPU timestamp counter */
        rq->current = next;
        rq->total_switches++;
    } else {
        /* No ready threads - idle */
        rq->current = NULL;
    }
    
    return next;
}

/* Yield current thread */
void sched_yield(uint32_t cpu_id) {
    cpu_runqueue_t *rq = &g_scheduler.runqueues[cpu_id];
    
    if (rq->current) {
        rq->current->state = THREAD_STATE_READY;
        enqueue_thread(rq, rq->current);
        rq->current = NULL;
    }
}

/* Block current thread (waiting for I/O, mutex, etc.) */
void sched_block(uint32_t cpu_id) {
    cpu_runqueue_t *rq = &g_scheduler.runqueues[cpu_id];
    
    if (rq->current) {
        rq->current->state = THREAD_STATE_BLOCKED;
        rq->current = NULL;
    }
}

/* Unblock thread and make it ready */
void sched_unblock(thread_t *thread) {
    if (!thread || thread->state != THREAD_STATE_BLOCKED) return;
    
    /* Re-enqueue on best available CPU */
    thread->cpu_id = find_best_cpu(thread);
    enqueue_thread(&g_scheduler.runqueues[thread->cpu_id], thread);
}

/* Work stealing for load balancing */
static thread_t *steal_thread(cpu_runqueue_t *thief_rq) {
    uint32_t num_cpus = atomic_load(&g_scheduler.num_cpus);
    uint32_t thief_load = atomic_load(&thief_rq->num_threads);
    
    /* Look for CPU with significantly more threads */
    for (uint32_t i = 0; i < num_cpus; i++) {
        if (i == thief_rq->cpu_id) continue;
        
        cpu_runqueue_t *victim_rq = &g_scheduler.runqueues[i];
        uint32_t victim_load = atomic_load(&victim_rq->num_threads);
        
        /* Only steal if victim has at least 2 more threads */
        if (victim_load < thief_load + 2) continue;
        
        /* Try to steal a thread that can run on thief CPU */
        acquire_runqueue_lock(victim_rq);
        
        for (int prio = 0; prio <= MAX_PRIORITY; prio++) {
            thread_t *t = victim_rq->queues[prio];
            thread_t *prev = NULL;
            
            while (t) {
                if (cpu_in_affinity(t, thief_rq->cpu_id)) {
                    /* Found candidate - remove from victim queue */
                    if (prev) {
                        prev->next = t->next;
                    } else {
                        victim_rq->queues[prio] = t->next;
                    }
                    
                    if (t->next) {
                        t->next->prev = prev;
                    }
                    
                    /* Update bitmap if queue empty */
                    if (!victim_rq->queues[prio]) {
                        uint32_t word = prio / 32;
                        uint32_t bit = prio % 32;
                        atomic_fetch_and(&victim_rq->queue_bitmap[word], ~(1U << bit));
                    }
                    
                    atomic_fetch_sub(&victim_rq->num_threads, 1);
                    t->next = t->prev = NULL;
                    
                    release_runqueue_lock(victim_rq);
                    return t;
                }
                
                prev = t;
                t = t->next;
            }
        }
        
        release_runqueue_lock(victim_rq);
    }
    
    return NULL;
}

/* Periodic load balancing */
void sched_balance_load(uint32_t cpu_id) {
    cpu_runqueue_t *rq = &g_scheduler.runqueues[cpu_id];
    
    /* If we're idle or lightly loaded, try to steal work */
    if (atomic_load(&rq->num_threads) < 2) {
        thread_t *stolen = steal_thread(rq);
        if (stolen) {
            stolen->cpu_id = cpu_id;
            enqueue_thread(rq, stolen);
        }
    }
}