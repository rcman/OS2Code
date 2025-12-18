# SimpleOS

## Project Overview

SimpleOS v0.3 is a 32-bit x86 operating system kernel with virtual memory support. It's a multiboot-compliant kernel written primarily in C with x86 assembly, designed to boot on QEMU or real hardware via GRUB.

## Build and Run Commands

### Building
```bash
make                  # Build kernel.bin
make clean            # Remove all build artifacts
make info             # Show build configuration
```

### Running in QEMU
```bash
make run              # Run with GUI display (default)
make run-curses       # Run in curses mode (headless-friendly)
make run-nographic    # Run with serial output only
```

### Debugging
```bash
make debug            # Start QEMU with GDB server on localhost:1234
# Then in separate terminal: gdb kernel.bin
# (gdb) target remote localhost:1234
```

### Creating Bootable ISO
```bash
make iso              # Create simpleos.iso using grub-mkrescue
```

## Architecture Overview

### Boot Process Flow
1. **boot.asm** - Multiboot entry point, sets up initial stack, calls `kmain()`
2. **kernel.c:kmain()** - Main kernel initialization:
   - VGA text mode initialization
   - GDT (Global Descriptor Table) setup
   - IDT (Interrupt Descriptor Table) and PIC remapping
   - Physical Memory Manager (PMM) initialization from multiboot memory map
   - Virtual Memory Manager (VMM) initialization (paging enabled here)
   - Timer, keyboard, mouse, and syscall initialization
   - RamFS filesystem initialization
   - Event loop for shell

### Memory Management Architecture

**Physical Memory Manager (pmm.c/pmm.h)**
- Bitmap-based page frame allocator
- Initialized from multiboot memory map or fallback 64MB assumption
- Tracks 4KB pages, handles allocation/deallocation
- Key functions: `pmm_alloc_page()`, `pmm_free_page()`, `pmm_mark_region_used()`

**Virtual Memory Manager (vmm.c/vmm.h)**
- Two-level paging (page directory + page tables)
- Identity-maps first 4MB for kernel code/data
- Page fault handler registered at ISR 14
- Key functions: `vmm_map_page()`, `vmm_unmap_page()`, `vmm_alloc_page()`
- Flags: `PTE_PRESENT`, `PTE_WRITABLE`, `PTE_USER`

**Memory Layout**
- Kernel loaded at 1MB physical (0x100000) per multiboot spec
- Stack: 32KB reserved in boot.asm
- Kernel end marked by `_kernel_end` symbol from linker.ld
- PMM bitmap placed after kernel end
- Virtual address space starts identity-mapped

### Interrupt Architecture

**IDT Setup (idt.c)**
- 256 interrupt descriptor table entries
- CPU exceptions (0-31): ISR stubs in isr.asm
- IRQs (32-47): PIC remapped from default (IRQ 0-15 → INT 32-47)
- Syscall: INT 128 (0x80)
- Page faults: ISR 14 routed to `vmm_page_fault_handler()`

**Interrupt Handler Registration**
Use `register_interrupt_handler(uint8_t n, isr_t handler)` to install custom handlers. Handler receives `registers_t*` with CPU state.

**IRQ Handlers**
- IRQ0 (Timer): timer.c - 100Hz PIT timer
- IRQ1 (Keyboard): keyboard.c - PS/2 keyboard with scancode translation
- IRQ12 (Mouse): mouse.c - PS/2 mouse support

### Event System (events.c/events.h)

Simple event queue for decoupling input hardware from kernel logic:
- `push_event()` - Called by keyboard/mouse IRQ handlers
- `pop_event()` - Called by kernel main loop
- Event types: `EVENT_TYPE_KEY_DOWN`, `EVENT_TYPE_MOUSE_MOVE`, `EVENT_TYPE_MOUSE_CLICK`

The kernel main loop polls `events_pending()` and processes events to update the shell.

### Filesystem (ramfs.c/ramfs.h)

RamFS is a simple in-memory filesystem:
- Fixed array of 32 files max
- 32-byte filename limit
- 4KB max file size
- No directories, no persistence
- API: `ramfs_create()`, `ramfs_write()`, `ramfs_read()`, `ramfs_delete()`

### Shell Implementation (kernel.c)

The kernel implements a basic command-line shell in the main event loop:
- Command buffer: 256 bytes
- Commands: help, clear, info, mem, vmm, alloc, ls, touch, cat, write, rm, df, reboot, halt
- Prompt: `kernel> `
- Color-coded output using VGA text mode colors

## Compilation Requirements

**Toolchain**
- `gcc` with 32-bit support (`-m32` flag)
- `nasm` for assembly (ELF32 output)
- `ld` for linking (elf_i386 emulation)
- `qemu-system-i386` for testing
- `grub-mkrescue` for ISO creation (optional)

**Compiler Flags**
- `-m32 -ffreestanding -fno-stack-protector -fno-pic -nostdlib`
- `-fno-builtin -fno-exceptions`
- `-O2` optimization enabled

**32-bit Compilation on 64-bit Systems**
If you encounter link errors about missing 32-bit libraries, install:
- Fedora/RHEL: `sudo dnf install gcc.i686 glibc-devel.i686`
- Debian/Ubuntu: `sudo apt install gcc-multilib`

## Adding New Features

**Adding a new interrupt handler:**
1. Add ISR stub in isr.asm following existing pattern
2. Add `extern void isrN(void);` in idt.c
3. Call `idt_set_gate(N, ...)` in `idt_init()`
4. Register handler with `register_interrupt_handler(N, handler_func)`

**Adding a new shell command:**
1. Add command parsing in `shell_process_command()` in kernel.c
2. Follow existing patterns: `str_equal()` for exact match, `str_starts_with()` for arguments
3. Parse arguments using `skip_whitespace()` and `skip_to_whitespace()`

**Adding a new device driver:**
1. Create driver.c and driver.h files
2. Add initialization call in `kmain()` before `enable_interrupts()`
3. If hardware interrupt is needed, register IRQ handler in idt.c
4. Use `push_event()` to send events to the main loop if needed

## Testing Memory Management

Use the `alloc` shell command to test the page allocation system:
```
kernel> alloc
```
This will allocate a physical page, map it to virtual address 0x400000, write test data, verify the write, then unmap and free the page.

Use `mem` to view PMM statistics and `vmm` to see current page mappings.

## Testing User Mode (Ring 3)

The kernel now supports user mode execution. Use the `usermode` shell command to test privilege level transitions:
```
kernel> usermode
```

**What this test does:**
1. Allocates a user mode stack at 0x800000
2. Prepares code page with user-accessible permissions
3. Executes `iret` instruction to jump from Ring 0 (kernel) to Ring 3 (user)
4. User mode code makes a system call via `int 0x80`
5. System call handler (Ring 0) receives the call and verifies the privilege level
6. Test passes if syscall shows `CS = 0x1b (Ring 3)`

This demonstrates:
- GDT configuration with user code/data segments
- TSS (Task State Segment) for privilege transitions
- Page table permissions (PTE_USER flag)
- System call interface for user→kernel communication

**Implementation files:**
- `usermode.c`/`usermode.h` - User mode test code
- `gdt.c` - GDT entries 3-4 are user code/data segments (Ring 3)
- `syscall.c` - INT 0x80 handler checks for user mode test syscall (0x01)
- `vmm.c` - Identity-mapped 4MB region marked with PTE_USER flag

## Linker Script (linker.ld)

The kernel is linked at 1MB (0x100000) with sections in order:
1. .multiboot - Must be in first 8KB for GRUB
2. .text - Code section
3. .rodata - Read-only data
4. .data - Initialized data
5. .bss - Uninitialized data
6. .kernel_end - End marker, aligned to 4K page boundary

The `_kernel_end` symbol is critical for PMM initialization.

## Common Gotchas

**Page Fault Handler Must Be Registered Early**
The page fault handler (ISR 14) must be registered in `kmain()` BEFORE calling `vmm_init()` which enables paging. Otherwise, any page fault during initialization will triple fault.

**Multiboot Memory Map**
When running with `-kernel` flag in QEMU, multiboot info may be minimal. The code has a fallback that assumes 64MB RAM when no memory map is provided.

**Identity Mapping Required**
The kernel code executing at physical address 1MB must remain identity-mapped (virtual = physical) because the code doesn't use higher-half kernel addressing. Changing this requires updating the linker script and entry point.

**No Standard Library**
This is a freestanding environment. No malloc, printf from libc, or standard headers. The kernel implements its own `printf()` in printf.c using variable arguments and VGA text output.
