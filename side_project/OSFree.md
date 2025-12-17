# OSFree Multicore Architecture Design Document

## Executive Summary

This document outlines the modernization of OSFree to support modern multicore CPUs and hardware while maintaining full backward compatibility with OS/2 applications (16-bit and 32-bit).

## Design Goals

1. **Multicore Support**: Utilize all available CPU cores efficiently
2. **Modern Hardware**: Support UEFI, NVMe, USB 3.x, PCIe 4.0+, etc.
3. **OS/2 Compatibility**: Run original OS/2 applications without modification
4. **Performance**: Match or exceed modern operating systems
5. **Scalability**: Support up to 256 CPU cores and NUMA systems

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                   OS/2 Applications                          │
│  (16-bit, 32-bit, DOS, Win16 via compatibility layers)      │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│              OS/2 API Compatibility Layer                    │
│  (DosXXX calls, 16→32-bit thunking, Workplace Shell)        │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│                  OSFree Personality Layer                    │
│  - Process Management    - File System (JFS, FAT, ext4)     │
│  - Memory Management     - Device I/O                        │
│  - IPC (Named Pipes, Queues, Semaphores)                    │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│              SMP Scheduler & Thread Management               │
│  - Per-CPU run queues    - NUMA awareness                   │
│  - Priority inheritance  - Work stealing                     │
│  - OS/2 priority classes - Real-time support                │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│                  L4 Microkernel (Modified)                   │
│  - IPC primitives        - Memory protection                 │
│  - Thread management     - Interrupt handling                │
│  - SMP synchronization   - Address spaces                    │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│              Hardware Abstraction Layer (HAL)                │
│  - ACPI/UEFI            - Modern timer sources (TSC, HPET)  │
│  - APIC/x2APIC          - DMA remapping (IOMMU)             │
│  - NUMA topology        - CPU feature detection              │
└─────────────────────────────────────────────────────────────┘
                           │
┌─────────────────────────────────────────────────────────────┐
│              Modern Device Drivers                           │
│  - NVMe Storage         - USB 3.x/4.0                        │
│  - PCIe 4.0/5.0         - Modern network (10GbE+)           │
│  - GPU (VESA fallback)  - Sound (HDA)                        │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. SMP Scheduler

**Key Features:**
- Per-CPU run queues with 128 priority levels
- OS/2 priority class mapping (Idle, Regular, Server, Time-Critical)
- Lock-free queue operations using atomic operations
- Work stealing for load balancing
- CPU affinity and NUMA node preference
- Preemptive multitasking with configurable time slices

**Priority Mapping:**
```
OS/2 Class              Priority Range    Typical Use
─────────────────────────────────────────────────────────
Idle Time               0-31             Background tasks
Regular                 32-63            Normal applications
Foreground Server       64-95            Interactive servers
Time Critical           96-127           Real-time tasks
```

**Scheduling Algorithm:**
1. Each CPU maintains independent run queues
2. Highest priority thread always runs first
3. Round-robin within same priority
4. Periodic load balancing (every 50ms)
5. Work stealing when CPU becomes idle

### 2. Hardware Abstraction Layer (HAL)

**CPU Management:**
- CPUID-based feature detection (SSE4, AVX2, AVX-512, etc.)
- Per-CPU data structures (GDT, IDT, TSS)
- APIC/x2APIC for interrupt routing
- TSC synchronization across cores
- CPU topology detection (cores, threads, cache hierarchy)

**Memory Management:**
- UEFI memory map parsing
- NUMA node detection via ACPI SRAT tables
- Page table management (4-level paging for x86-64)
- Large page support (2MB/1GB pages)
- Memory type range registers (MTRR) for device memory

**Interrupt Management:**
- MSI/MSI-X support for modern devices
- Per-CPU interrupt stacks
- Interrupt affinity for load distribution
- Fast interrupt delivery via posted interrupts

**Power Management:**
- ACPI support (C-states, P-states, T-states)
- CPU frequency scaling
- Core parking for power savings
- Thermal management

### 3. OS/2 Compatibility Layer

**16-bit to 32-bit Thunking:**
- Automatic call translation for mixed-mode apps
- Segment to flat memory model conversion
- Real-mode emulation for DOS sessions

**API Implementation:**
```
DosXXX Family:
- DosCreateThread    → Maps to native thread creation
- DosSetPriority     → Uses SMP scheduler priority system
- DosAllocMem        → Modern virtual memory allocation
- DosSleep           → Timer-based sleep with scheduler yield
- DosRead/DosWrite   → VFS layer with modern I/O

Workplace Shell:
- PM (Presentation Manager) → Modern GUI subsystem
- SOM (System Object Model)  → Object-oriented framework
```

**File System Support:**
- JFS (native OS/2 file system)
- FAT12/16/32 for compatibility
- ext4 (read/write for Linux interop)
- NTFS (read-only for Windows interop)
- Network file systems (SMB, NFS)

### 4. Device Driver Framework

**Driver Model:**
```c
/* Modern driver interface */
typedef struct device_driver {
    char name[64];
    uint32_t version;
    
    /* Lifecycle */
    int (*probe)(struct device *dev);
    int (*remove)(struct device *dev);
    int (*suspend)(struct device *dev);
    int (*resume)(struct device *dev);
    
    /* Operations */
    int (*open)(struct device *dev);
    int (*close)(struct device *dev);
    ssize_t (*read)(struct device *dev, void *buf, size_t len);
    ssize_t (*write)(struct device *dev, const void *buf, size_t len);
    int (*ioctl)(struct device *dev, uint32_t cmd, void *arg);
    
    /* Interrupt handling */
    int (*irq_handler)(struct device *dev, uint32_t irq);
    
    /* DMA support */
    void *(*dma_alloc)(struct device *dev, size_t size, uint64_t *phys_addr);
    void (*dma_free)(struct device *dev, void *virt, size_t size);
} device_driver_t;
```

**Driver Wrapping:**
- Wrap Linux drivers where possible (network, storage)
- Native drivers for critical components
- Userspace driver support for safety

### 5. Boot Process

**UEFI Boot Sequence:**
```
1. UEFI Firmware
   ↓
2. OSFree UEFI Bootloader (replaces FreeLDR)
   - Load kernel image
   - Parse ACPI tables
   - Set up initial page tables
   - Detect CPU topology
   ↓
3. Kernel Initialization
   - Initialize BSP (Bootstrap Processor)
   - Set up HAL
   - Initialize memory manager
   - Start SMP initialization
   ↓
4. SMP Initialization
   - Wake up Application Processors (APs)
   - Initialize per-CPU structures
   - Start scheduler on each CPU
   ↓
5. Device Initialization
   - Enumerate PCIe devices
   - Load drivers
   - Mount root filesystem
   ↓
6. OS/2 Personality
   - Start OS/2 subsystem
   - Load Workplace Shell
   - Start user session
```

## Memory Management

### Virtual Memory Layout (x86-64)

```
0x0000000000000000 - 0x00007FFFFFFFFFFF  User space (128TB)
  0x0000000000000000 - 0x0000000000400000  Null guard page
  0x0000000000400000 - 0x00007FFFFFFFFFFF  Application space

0xFFFF800000000000 - 0xFFFFFFFFFFFFFFFF  Kernel space (128TB)
  0xFFFF800000000000 - 0xFFFF880000000000  Physical memory mapping
  0xFFFF880000000000 - 0xFFFFC80000000000  vmalloc/ioremap space
  0xFFFFC80000000000 - 0xFFFFE80000000000  Per-CPU data
  0xFFFFE80000000000 - 0xFFFFFFFF80000000  Kernel heap
  0xFFFFFFFF80000000 - 0xFFFFFFFFFFFFFFFF  Kernel code/data
```

### NUMA-Aware Allocation

- Allocate memory from thread's local NUMA node
- Page migration for threads that move between nodes
- Interleaved allocation for shared memory

## Performance Optimizations

### Cache Optimization

1. **Per-CPU Data Structures**: Minimize cache line bouncing
2. **Lock-Free Algorithms**: Use atomic operations instead of locks
3. **Cache Line Alignment**: Align hot data to 64-byte boundaries
4. **Prefetching**: Software prefetch for predictable access patterns

### System Call Optimization

1. **Fast System Calls**: Use SYSCALL/SYSENTER instead of INT 0x80
2. **vDSO**: Map frequently-used syscalls into user space
3. **Batching**: Batch multiple operations into single kernel entry

### Thread Switching

1. **Lazy FPU Save**: Only save FPU state if thread used it
2. **TLB Optimization**: Use PCIDs to avoid TLB flush on context switch
3. **Stack Caching**: Reuse kernel stacks for short-lived threads

## Testing Strategy

### Compatibility Testing

- Run OS/2 Warp 4 application suite
- Test DOS and Win16 sessions
- Verify Workplace Shell functionality
- Database applications (DB2)
- Legacy business software

### Performance Testing

- Context switch latency
- Thread creation/destruction speed
- Scheduler fairness
- Scalability (1-256 cores)
- Memory bandwidth (NUMA)
- I/O throughput (NVMe)

### Stress Testing

- High thread count (10,000+ threads)
- CPU burn tests
- Memory pressure tests
- I/O saturation tests
- Interrupt flood tests

## Implementation Roadmap

### Phase 1: Foundation (Months 1-3)
- ✅ SMP scheduler core
- HAL implementation (ACPI, APIC, CPU detection)
- Basic memory management
- UEFI bootloader

### Phase 2: Device Support (Months 4-6)
- NVMe driver
- USB 3.x support
- Modern network drivers
- Graphics (VESA + basic GPU)

### Phase 3: OS/2 Compatibility (Months 7-9)
- DosXXX API implementation
- 16-bit thunking layer
- File system support (JFS, FAT)
- Process/thread management

### Phase 4: Optimization (Months 10-12)
- Performance tuning
- NUMA optimization
- Power management
- Extensive testing

### Phase 5: User Experience (Months 13-15)
- Workplace Shell
- Presentation Manager
- Modern utilities
- Documentation

## Building and Development

### Build System

Use CMake for modern build management:

```cmake
# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

# Project
project(OSFree C ASM)

# Compiler flags
set(CMAKE_C_FLAGS "-std=c11 -ffreestanding -nostdlib -mno-red-zone")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mcmodel=kernel -mno-sse")

# Build targets
add_executable(osfree.elf
    kernel/main.c
    kernel/sched.c
    kernel/hal.c
    # ... more files
)
```

### Development Tools

- **Compiler**: GCC 11+ or Clang 13+
- **Assembler**: NASM or GNU AS
- **Debugger**: GDB with QEMU support
- **Virtualization**: QEMU/KVM for testing
- **Version Control**: Git

### Debugging

- Serial port debugging
- QEMU GDB stub
- Built-in kernel debugger
- Trace points and performance counters

## Security Considerations

### Memory Protection

- NX (No Execute) bit enforcement
- SMEP/SMAP (Supervisor Mode Execution/Access Prevention)
- KASLR (Kernel Address Space Layout Randomization)
- Guard pages between kernel stacks

### Process Isolation

- Separate address spaces per process
- Capability-based security (inherited from L4)
- Mandatory Access Control (optional)

### Driver Safety

- Drivers in separate protection domains
- IOMMU for DMA isolation
- Fault isolation (driver crash doesn't kill kernel)

## Future Enhancements

### Advanced Features

- Real-time scheduling (SCHED_FIFO, SCHED_RR)
- CPU hotplug support
- Live kernel patching
- Container support
- Virtualization (KVM-style)

### Modern APIs

- POSIX compatibility layer
- Linux syscall emulation
- Windows subsystem (WSL-style)

## References

- OS/2 Warp 4 API Reference
- L4 Microkernel Specification
- Intel Software Developer Manual
- ACPI Specification
- UEFI Specification
- Linux kernel source (for driver reference)

## Contributors

This is an open-source project. Contributions welcome!

## License

GPL v2 (consistent with original OSFree license)