/*
 * OSFree Hardware Abstraction Layer (HAL)
 * 
 * Provides abstraction for:
 * - CPU detection and initialization (multicore)
 * - APIC/x2APIC interrupt controller
 * - ACPI tables parsing
 * - NUMA topology detection
 * - Modern timer sources (TSC, HPET)
 * - UEFI interface
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ============================================
 * CPU Feature Detection
 * ============================================ */

typedef struct cpu_features {
    /* Basic features */
    bool fpu;           /* x87 FPU */
    bool tsc;           /* Time Stamp Counter */
    bool msr;           /* Model Specific Registers */
    bool apic;          /* APIC on chip */
    bool mtrr;          /* Memory Type Range Registers */
    
    /* SIMD */
    bool sse;
    bool sse2;
    bool sse3;
    bool ssse3;
    bool sse4_1;
    bool sse4_2;
    bool avx;
    bool avx2;
    bool avx512f;
    
    /* Advanced features */
    bool x2apic;        /* Extended APIC */
    bool pcid;          /* Process Context Identifiers */
    bool invpcid;       /* INVPCID instruction */
    bool smep;          /* Supervisor Mode Execution Prevention */
    bool smap;          /* Supervisor Mode Access Prevention */
    bool gbpages;       /* 1GB pages */
    bool rdtscp;        /* RDTSCP instruction */
    bool invariant_tsc; /* TSC doesn't change with C-states */
    bool hypervisor;    /* Running under hypervisor */
    
    /* Vendor info */
    char vendor[13];
    uint32_t family;
    uint32_t model;
    uint32_t stepping;
    uint32_t max_cpuid;
} cpu_features_t;

/* CPUID wrapper */
static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx, 
                         uint32_t *ecx, uint32_t *edx) {
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(subleaf));
}

void hal_detect_cpu_features(cpu_features_t *features) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Get vendor string */
    cpuid(0, 0, &eax, &ebx, &ecx, &edx);
    features->max_cpuid = eax;
    *(uint32_t *)(features->vendor + 0) = ebx;
    *(uint32_t *)(features->vendor + 4) = edx;
    *(uint32_t *)(features->vendor + 8) = ecx;
    features->vendor[12] = '\0';
    
    /* Feature flags (leaf 1) */
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    
    features->family = ((eax >> 8) & 0xF) + ((eax >> 20) & 0xFF);
    features->model = ((eax >> 4) & 0xF) | (((eax >> 16) & 0xF) << 4);
    features->stepping = eax & 0xF;
    
    /* EDX features */
    features->fpu = (edx & (1 << 0)) != 0;
    features->tsc = (edx & (1 << 4)) != 0;
    features->msr = (edx & (1 << 5)) != 0;
    features->apic = (edx & (1 << 9)) != 0;
    features->mtrr = (edx & (1 << 12)) != 0;
    features->sse = (edx & (1 << 25)) != 0;
    features->sse2 = (edx & (1 << 26)) != 0;
    
    /* ECX features */
    features->sse3 = (ecx & (1 << 0)) != 0;
    features->ssse3 = (ecx & (1 << 9)) != 0;
    features->sse4_1 = (ecx & (1 << 19)) != 0;
    features->sse4_2 = (ecx & (1 << 20)) != 0;
    features->x2apic = (ecx & (1 << 21)) != 0;
    features->avx = (ecx & (1 << 28)) != 0;
    features->hypervisor = (ecx & (1 << 31)) != 0;
    
    /* Extended features (leaf 7) */
    if (features->max_cpuid >= 7) {
        cpuid(7, 0, &eax, &ebx, &ecx, &edx);
        features->avx2 = (ebx & (1 << 5)) != 0;
        features->smep = (ebx & (1 << 7)) != 0;
        features->avx512f = (ebx & (1 << 16)) != 0;
        features->smap = (ebx & (1 << 20)) != 0;
        features->invpcid = (ebx & (1 << 10)) != 0;
    }
    
    /* Extended features (leaf 0x80000001) */
    cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
    features->gbpages = (edx & (1 << 26)) != 0;
    features->rdtscp = (edx & (1 << 27)) != 0;
    features->pcid = (ecx & (1 << 1)) != 0;
    
    /* Invariant TSC (leaf 0x80000007) */
    cpuid(0x80000007, 0, &eax, &ebx, &ecx, &edx);
    features->invariant_tsc = (edx & (1 << 8)) != 0;
}

/* ============================================
 * CPU Topology Detection
 * ============================================ */

typedef struct cpu_topology {
    uint32_t num_cores;         /* Physical cores */
    uint32_t num_threads;       /* Logical processors */
    uint32_t num_packages;      /* Physical CPUs/sockets */
    uint32_t threads_per_core;  /* SMT level */
    uint32_t cores_per_package;
    
    /* Per-CPU info */
    struct {
        uint32_t apic_id;
        uint32_t package_id;
        uint32_t core_id;
        uint32_t thread_id;
        uint32_t numa_node;
    } cpus[256];
} cpu_topology_t;

void hal_detect_cpu_topology(cpu_topology_t *topo) {
    uint32_t eax, ebx, ecx, edx;
    
    /* Get thread and core count from CPUID */
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    uint32_t logical_processors = (ebx >> 16) & 0xFF;
    
    cpuid(4, 0, &eax, &ebx, &ecx, &edx);
    uint32_t cores_per_package = ((eax >> 26) & 0x3F) + 1;
    
    topo->num_threads = logical_processors;
    topo->cores_per_package = cores_per_package;
    topo->threads_per_core = logical_processors / cores_per_package;
    topo->num_packages = 1;  /* Will be updated by ACPI */
    topo->num_cores = cores_per_package;
    
    /* Get APIC ID for this CPU */
    cpuid(1, 0, &eax, &ebx, &ecx, &edx);
    uint32_t initial_apic_id = (ebx >> 24) & 0xFF;
    
    /* Parse topology from x2APIC if available */
    cpuid(0xB, 0, &eax, &ebx, &ecx, &edx);
    if (ebx != 0) {
        /* Extended topology enumeration available */
        uint32_t shift = eax & 0x1F;
        uint32_t thread_mask = (1U << shift) - 1;
        
        cpuid(0xB, 1, &eax, &ebx, &ecx, &edx);
        uint32_t core_shift = eax & 0x1F;
        uint32_t core_mask = (1U << core_shift) - 1;
        
        for (uint32_t i = 0; i < topo->num_threads; i++) {
            uint32_t apic = topo->cpus[i].apic_id;
            topo->cpus[i].thread_id = apic & thread_mask;
            topo->cpus[i].core_id = (apic >> shift) & (core_mask >> shift);
            topo->cpus[i].package_id = apic >> core_shift;
        }
    }
}

/* ============================================
 * APIC/x2APIC Management
 * ============================================ */

#define APIC_BASE_MSR           0x1B
#define APIC_ID                 0x20
#define APIC_EOI                0xB0
#define APIC_SPURIOUS           0xF0
#define APIC_ICR_LOW            0x300
#define APIC_ICR_HIGH           0x310
#define APIC_TIMER_LVT          0x320
#define APIC_TIMER_INITIAL      0x380
#define APIC_TIMER_CURRENT      0x390
#define APIC_TIMER_DIVIDE       0x3E0

static uint64_t apic_base_phys;
static void *apic_base_virt;
static bool x2apic_mode = false;

/* MSR access */
static inline uint64_t rdmsr(uint32_t msr) {
    uint32_t low, high;
    __asm__ volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

static inline void wrmsr(uint32_t msr, uint64_t value) {
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    __asm__ volatile("wrmsr" :: "a"(low), "d"(high), "c"(msr));
}

/* APIC register access */
static inline uint32_t apic_read(uint32_t reg) {
    if (x2apic_mode) {
        return rdmsr(0x800 + (reg >> 4));
    } else {
        return *(volatile uint32_t *)((uintptr_t)apic_base_virt + reg);
    }
}

static inline void apic_write(uint32_t reg, uint32_t value) {
    if (x2apic_mode) {
        wrmsr(0x800 + (reg >> 4), value);
    } else {
        *(volatile uint32_t *)((uintptr_t)apic_base_virt + reg) = value;
    }
}

void hal_apic_init(cpu_features_t *features) {
    /* Get APIC base address */
    uint64_t apic_base = rdmsr(APIC_BASE_MSR);
    apic_base_phys = apic_base & 0xFFFFF000;
    
    /* Enable x2APIC if supported */
    if (features->x2apic) {
        wrmsr(APIC_BASE_MSR, apic_base | (1 << 11) | (1 << 10));
        x2apic_mode = true;
    } else {
        /* Map APIC registers if using xAPIC mode */
        /* This would use your memory manager to map the physical address */
        /* apic_base_virt = map_physical(apic_base_phys, 4096); */
        wrmsr(APIC_BASE_MSR, apic_base | (1 << 11));
    }
    
    /* Enable APIC */
    uint32_t spurious = apic_read(APIC_SPURIOUS);
    apic_write(APIC_SPURIOUS, spurious | 0x1FF);  /* Enable + spurious vector 0xFF */
}

uint32_t hal_apic_get_id(void) {
    if (x2apic_mode) {
        return rdmsr(0x802);  /* x2APIC ID MSR */
    } else {
        return apic_read(APIC_ID) >> 24;
    }
}

void hal_apic_send_eoi(void) {
    apic_write(APIC_EOI, 0);
}

void hal_apic_send_ipi(uint32_t dest_apic_id, uint32_t vector) {
    if (x2apic_mode) {
        wrmsr(0x830, ((uint64_t)dest_apic_id << 32) | vector);
    } else {
        apic_write(APIC_ICR_HIGH, dest_apic_id << 24);
        apic_write(APIC_ICR_LOW, vector);
    }
}

/* ============================================
 * ACPI Table Parsing
 * ============================================ */

typedef struct acpi_rsdp {
    char signature[8];
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;
    uint32_t rsdt_address;
    
    /* ACPI 2.0+ */
    uint32_t length;
    uint64_t xsdt_address;
    uint8_t extended_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp_t;

typedef struct acpi_header {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

typedef struct acpi_madt {
    acpi_header_t header;
    uint32_t local_apic_address;
    uint32_t flags;
    uint8_t entries[];
} __attribute__((packed)) acpi_madt_t;

typedef struct acpi_madt_lapic {
    uint8_t type;           /* 0 = Local APIC */
    uint8_t length;
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t flags;
} __attribute__((packed)) acpi_madt_lapic_t;

typedef struct acpi_srat {
    acpi_header_t header;
    uint32_t reserved1;
    uint64_t reserved2;
    uint8_t entries[];
} __attribute__((packed)) acpi_srat_t;

typedef struct acpi_srat_lapic {
    uint8_t type;           /* 0 = Local APIC affinity */
    uint8_t length;
    uint8_t proximity_domain_low;
    uint8_t apic_id;
    uint32_t flags;
    uint8_t local_sapic_eid;
    uint8_t proximity_domain_high[3];
    uint32_t clock_domain;
} __attribute__((packed)) acpi_srat_lapic_t;

void *hal_acpi_find_table(const char *signature) {
    /* This would search for ACPI tables in memory */
    /* Implementation depends on UEFI providing tables */
    return NULL;  /* Placeholder */
}

void hal_parse_madt(cpu_topology_t *topo) {
    acpi_madt_t *madt = hal_acpi_find_table("APIC");
    if (!madt) return;
    
    uint8_t *ptr = madt->entries;
    uint8_t *end = (uint8_t *)madt + madt->header.length;
    
    uint32_t cpu_count = 0;
    
    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];
        
        if (type == 0) {  /* Local APIC */
            acpi_madt_lapic_t *lapic = (acpi_madt_lapic_t *)ptr;
            
            if (lapic->flags & 1) {  /* Enabled */
                topo->cpus[cpu_count].apic_id = lapic->apic_id;
                cpu_count++;
            }
        }
        
        ptr += length;
    }
    
    topo->num_threads = cpu_count;
}

void hal_parse_srat(cpu_topology_t *topo) {
    acpi_srat_t *srat = hal_acpi_find_table("SRAT");
    if (!srat) return;
    
    uint8_t *ptr = srat->entries;
    uint8_t *end = (uint8_t *)srat + srat->header.length;
    
    while (ptr < end) {
        uint8_t type = ptr[0];
        uint8_t length = ptr[1];
        
        if (type == 0) {  /* Local APIC affinity */
            acpi_srat_lapic_t *affinity = (acpi_srat_lapic_t *)ptr;
            
            uint32_t domain = affinity->proximity_domain_low |
                             (affinity->proximity_domain_high[0] << 8) |
                             (affinity->proximity_domain_high[1] << 16) |
                             (affinity->proximity_domain_high[2] << 24);
            
            /* Find CPU with this APIC ID and set NUMA node */
            for (uint32_t i = 0; i < topo->num_threads; i++) {
                if (topo->cpus[i].apic_id == affinity->apic_id) {
                    topo->cpus[i].numa_node = domain;
                    break;
                }
            }
        }
        
        ptr += length;
    }
}

/* ============================================
 * Time Stamp Counter (TSC)
 * ============================================ */

static uint64_t tsc_frequency = 0;  /* Hz */

static inline uint64_t rdtsc(void) {
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

void hal_calibrate_tsc(void) {
    /* Calibrate TSC against a known timer source (HPET or PIT) */
    /* This is simplified - real implementation would use HPET */
    
    /* For now, try to get from CPUID if available */
    uint32_t eax, ebx, ecx, edx;
    cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);
    
    if (eax && ebx && ecx) {
        /* TSC frequency = ECX * EBX / EAX */
        tsc_frequency = ((uint64_t)ecx * ebx) / eax;
    } else {
        /* Fallback: calibrate manually using PIT or assume 2.4 GHz */
        tsc_frequency = 2400000000ULL;
    }
}

uint64_t hal_get_tsc_frequency(void) {
    return tsc_frequency;
}

uint64_t hal_get_nanoseconds(void) {
    uint64_t tsc = rdtsc();
    return (tsc * 1000000000ULL) / tsc_frequency;
}

/* ============================================
 * SMP Initialization
 * ============================================ */

extern void ap_startup_code(void);  /* Assembly trampoline */

void hal_start_ap(uint32_t apic_id, void (*entry_point)(void)) {
    /* Send INIT IPI */
    hal_apic_send_ipi(apic_id, 0x4500);  /* INIT, level assert */
    
    /* Wait 10ms */
    uint64_t start = hal_get_nanoseconds();
    while (hal_get_nanoseconds() - start < 10000000);
    
    /* Send STARTUP IPI */
    uint32_t vector = ((uintptr_t)ap_startup_code >> 12) & 0xFF;
    hal_apic_send_ipi(apic_id, 0x4600 | vector);  /* STARTUP */
    
    /* Wait 200us */
    start = hal_get_nanoseconds();
    while (hal_get_nanoseconds() - start < 200000);
    
    /* Send second STARTUP IPI (per Intel spec) */
    hal_apic_send_ipi(apic_id, 0x4600 | vector);
}

void hal_smp_init(cpu_topology_t *topo, void (*ap_entry)(void)) {
    uint32_t bsp_apic_id = hal_apic_get_id();
    
    for (uint32_t i = 0; i < topo->num_threads; i++) {
        if (topo->cpus[i].apic_id != bsp_apic_id) {
            hal_start_ap(topo->cpus[i].apic_id, ap_entry);
        }
    }
}

/* ============================================
 * HAL Initialization
 * ============================================ */

typedef struct hal_info {
    cpu_features_t features;
    cpu_topology_t topology;
    uint64_t tsc_frequency;
    bool x2apic_enabled;
} hal_info_t;

static hal_info_t g_hal_info = {0};

hal_info_t *hal_initialize(void) {
    /* 1. Detect CPU features */
    hal_detect_cpu_features(&g_hal_info.features);
    
    /* 2. Initialize APIC */
    hal_apic_init(&g_hal_info.features);
    g_hal_info.x2apic_enabled = x2apic_mode;
    
    /* 3. Detect CPU topology */
    hal_detect_cpu_topology(&g_hal_info.topology);
    
    /* 4. Parse ACPI tables for detailed topology and NUMA */
    hal_parse_madt(&g_hal_info.topology);
    hal_parse_srat(&g_hal_info.topology);
    
    /* 5. Calibrate TSC */
    hal_calibrate_tsc();
    g_hal_info.tsc_frequency = tsc_frequency;
    
    return &g_hal_info;
}

hal_info_t *hal_get_info(void) {
    return &g_hal_info;
}