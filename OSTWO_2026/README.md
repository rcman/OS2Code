# OS/Two - OS/2-Compatible Operating System

**Version:** 0.3 Alpha
**Status:** Early Development
**License:** MIT (to be determined)

---

## What is OS/Two?

**OS/Two** is an open-source operating system designed to be compatible with IBM's OS/2 APIs and executable formats. It's a modern reimplementation that aims to run OS/2 applications while using a clean, modern kernel architecture.

The name "OS/Two" is a playful homage to OS/2, pronounced the same way but with a fresh identity.

---

## Current Status (v0.3 Alpha)

### ✅ Implemented Features

**Core Kernel:**
- 32-bit x86 protected mode with multiboot compliance
- GDT (Global Descriptor Table) with Ring 0/3 separation
- IDT (Interrupt Descriptor Table) with 256 entries
- CPU exception handlers (divide by zero, page fault, GPF, etc.)
- Physical Memory Manager (PMM) - bitmap-based page frame allocator
- Virtual Memory Manager (VMM) - two-level paging (page directory + page tables)
- PIT timer at 100 Hz with preemptive multitasking
- Real-Time Clock (RTC) support

**Graphics & Display:**
- VGA text mode (80x25, 16 colors)
- VGA graphics mode (320x200, 256 colors)
- **VBE (VESA BIOS Extensions) support for high-resolution graphics**
  - 1024x768x32 (true color) mode via GRUB multiboot
  - Framebuffer mapping in protected mode
  - 32-bit pixel operations with proper color handling
- GUI window manager with:
  - Draggable, resizable windows
  - Window titlebar with minimize/maximize/close buttons
  - Taskbar with Start Menu
  - **Flicker-free mouse cursor with proper save/restore** (fixed in this version)
  - Desktop background color customization

**Device Drivers:**
- PS/2 keyboard with scancode translation
- PS/2 mouse with button and movement tracking
- Serial ports (COM1/COM2) for debugging
- Parallel port support
- PC speaker for beep sounds
- IDE/ATA disk controller
- PCI bus enumeration and device detection
- VirtIO block device (for QEMU disk images)
- VirtIO network device (for QEMU networking)

**Process Management:**
- Multi-process support with process table
- Round-robin scheduler with time slicing (10ms quantum)
- Process states: READY, RUNNING, BLOCKED, TERMINATED
- Per-process page directories (isolated virtual address spaces)
- User mode (Ring 3) execution with privilege separation
- Parent-child process hierarchy
- Process creation (`DosExecPgm` API)
- ELF executable loader
- Priority-based scheduling

**Threading:**
- Multi-threading support within processes
- Thread creation and management APIs
- Thread-local storage
- Thread synchronization primitives

**Inter-Process Communication (IPC):**
- Semaphores (mutex, event, muxwait)
- Named pipes for process communication
- Message queues
- Shared memory regions

**File Systems:**
- **RamFS** - In-memory filesystem with:
  - Create, read, write, delete operations
  - 32 files maximum, 4KB per file
  - Directory listing
- **VFS (Virtual File System)** layer for multiple filesystem support
- DOS-style file I/O compatibility layer

**Networking:**
- Basic TCP/IP stack implementation
- Ethernet frame handling
- IP packet routing
- UDP protocol support
- TCP protocol (basic implementation)
- VirtIO network driver for QEMU

**DOS API (Implemented):**
- `DosWrite(handle, buffer, length)` - Write to file descriptor
- `DosRead(handle, buffer, length)` - Read from file descriptor
- `DosOpen(path, handle, flags)` - Open file
- `DosClose(handle)` - Close file handle
- `DosDelete(path)` - Delete file
- `DosExit(exitcode)` - Terminate process
- `DosExecPgm(objname, args, flags, env, results, path)` - Execute program
- `DosGetPID()` - Get current process ID
- `DosGetPPID()` - Get parent process ID
- `DosPutChar(c)` - Write single character to stdout
- `DosPutString(str)` - Write null-terminated string
- `DosAllocMem(size, flags)` - Allocate memory
- `DosFreeMem(address)` - Free memory
- `DosCreateThread(start_addr, stack_size, flags)` - Create thread
- `DosWaitThread(tid)` - Wait for thread termination
- `DosCreateMutexSem(name)` - Create mutex semaphore
- `DosCreateEventSem(name)` - Create event semaphore
- `DosRequestMutexSem(handle, timeout)` - Acquire mutex
- `DosReleaseMutexSem(handle)` - Release mutex
- `DosPostEventSem(handle)` - Post event
- `DosWaitEventSem(handle, timeout)` - Wait for event

**Linux Syscall Compatibility Layer:**
- Basic Linux syscall support for ported applications
- `read()`, `write()`, `open()`, `close()` wrappers
- `exit()`, `fork()`, `execve()` emulation

**System Calls:**
- INT 0x80 syscall interface with proper privilege transitions
- Inline syscall wrappers in `dosapi.h`
- Full Ring 3 → Ring 0 transition handling
- System call parameter validation

**Interactive Shell:**
- Command-line interface with 40+ commands:
  - **System:** `help`, `clear`, `info`, `reboot`, `halt`, `beep`
  - **Memory:** `mem`, `vmm`, `alloc`
  - **Process:** `ps`, `testproc`, `testsyscall`, `usermode`
  - **Files:** `ls`, `cat`, `touch`, `write`, `rm`, `df`
  - **Threads:** `testthread`
  - **IPC:** `testsem`, `testpipe`
  - **Phase 2 Tests:** `phase2`, `testmem`, `testapis`, `testintegration`
  - **Network:** `netinfo`, `nettest`
  - **GUI:** `gui`, `desktop` (color selection)
  - **Date/Time:** `date`, `time`
- Color-coded output
- Command history and editing

### 🚧 In Progress

- Full GUI application framework
- Network protocol stack completion
- Disk persistence for filesystems
- Extended threading APIs

### 📋 Planned Features

See [OSTWO_ROADMAP.md](OSTWO_ROADMAP.md) for the complete development plan including:
- LX executable loader (OS/2 native format)
- DOS compatibility box (INT 21h handler)
- Persistent filesystems (FAT16/FAT32, HPFS)
- Presentation Manager (full GUI)
- REXX scripting language
- SOM/WPS (Workplace Shell)

---

## Quick Start

### Prerequisites

**Required:**
- `gcc` with 32-bit support (`-m32` flag)
- `nasm` (Netwide Assembler)
- `ld` (GNU linker)
- `make`
- `qemu-system-i386` (for testing)

**Optional:**
- `grub-mkrescue` or `grub2-mkrescue` (for ISO creation)

**On Fedora/RHEL:**
```bash
sudo dnf install gcc nasm make qemu-system-x86 grub2-tools
```

**On Debian/Ubuntu:**
```bash
sudo apt install gcc nasm make qemu-system-x86 grub2-common xorriso
sudo apt install gcc-multilib  # For 32-bit support
```

### Building

```bash
make                  # Build kernel.bin
make clean            # Clean all build artifacts
make info             # Show build configuration
make iso              # Create bootable ISO (ostwo.iso)
```

### Running in QEMU

**Recommended - High Resolution Mode (1024x768x32):**
```bash
make run-iso          # Boot from ISO with VBE graphics (best experience)
```

**Alternative Run Targets:**
```bash
make run              # Direct kernel boot (basic display)
make run-hires        # Direct kernel boot with VBE support
make run-virtio-gpu   # VirtIO GPU for best performance
make run-curses       # Curses mode (headless-friendly)
make run-nographic    # Serial output only (no display)
```

**Debug Mode:**
```bash
make debug            # Start QEMU with GDB server on localhost:1234
# In another terminal:
gdb kernel.bin
(gdb) target remote localhost:1234
(gdb) break kmain
(gdb) continue
```

### Testing Shell Commands

Once booted, try these commands in the shell:

**System Information:**
```
help        - Show all available commands
info        - Display system information
mem         - Display memory statistics (PMM usage)
vmm         - Show virtual memory mappings
date        - Display current date
time        - Display current time
```

**Process Management:**
```
ps          - List all processes with state and priority
testproc    - Test multi-process scheduling (4 processes)
testsyscall - Test DOS API syscalls
usermode    - Test Ring 3 user mode transitions
```

**File System:**
```
ls          - List files in RamFS
cat <file>  - Display file contents
touch <file> - Create empty file
write <file> <text> - Write text to file
rm <file>   - Delete file
df          - Show filesystem statistics
```

**Threading & IPC:**
```
testthread  - Test multi-threading APIs
testsem     - Test semaphore synchronization
testpipe    - Test named pipes
```

**GUI & Graphics:**
```
gui         - Launch GUI window manager
desktop <color> - Set desktop background color (0-15)
```

**Network:**
```
netinfo     - Display network interface information
nettest     - Test network stack
```

**Integrated Tests:**
```
phase2      - Run full Phase 2 integration tests
testmem     - Test memory allocation APIs
testapis    - Test process and threading APIs
```

**System Control:**
```
clear       - Clear screen
beep        - Play a beep sound
reboot      - Reboot the system
halt        - Halt the CPU
```

---

## Architecture

### System Architecture

```
┌─────────────────────────────────────┐
│     User Mode Applications          │
│         (Ring 3)                    │
└──────────────┬──────────────────────┘
               │
          ┌────▼─────┐
          │ DOS API  │
          │(dosapi.h)│
          └────┬─────┘
               │
          ┌────▼─────┐
          │ INT 0x80 │
          │ Syscall  │
          └────┬─────┘
               │
┌──────────────▼──────────────────────┐
│      Kernel Mode (Ring 0)           │
├─────────────────────────────────────┤
│  Process Manager  │  Scheduler      │
│  Memory Manager   │  File Systems   │
│  Device Drivers   │  Network Stack  │
│  IPC & Sync       │  GUI Manager    │
└─────────────────────────────────────┘
```

### Memory Layout

```
0x00000000 - 0x003FFFFF : Kernel (identity-mapped, 4MB)
  ├── 0x00100000         : Kernel code start (1MB)
  ├── 0x00100000+        : .text (code)
  ├── ...                : .rodata (constants)
  ├── ...                : .data (initialized data)
  ├── ...                : .bss (uninitialized data)
  └── _kernel_end        : PMM bitmap starts here

0x00400000 - 0xBFFFEFFF : User space (per-process virtual memory)
  ├── 0x00400000         : User code/data
  ├── ...                : Heap (grows up)
  └── 0xBFFFF000         : User stack (grows down, 4KB)

0xC0000000 - 0xFDFFFFFF : Reserved for future use

Hardware-Mapped Regions:
  0xA0000 - 0xBFFFF     : VGA framebuffer (Mode 13h)
  0xB8000 - 0xBFFFF     : VGA text mode buffer
  0xE0000000+           : VBE framebuffer (if enabled, varies)
```

### Display Modes

**Text Mode (Default on startup):**
- 80x25 characters, 16 colors
- VGA text buffer at 0xB8000

**Graphics Mode 13h:**
- 320x200 pixels, 256 colors
- Linear framebuffer at 0xA0000

**VBE High-Resolution Mode (via ISO boot):**
- 1024x768 pixels, 32-bit color (16.7 million colors)
- Linear framebuffer (mapped via multiboot)
- Hardware-accelerated via GRUB

### Boot Process

1. **GRUB Bootloader** (when using ISO)
   - Sets up VBE framebuffer (1024x768x32)
   - Loads kernel.bin via multiboot protocol
   - Passes memory map and framebuffer info

2. **boot.asm** - Multiboot entry point
   - Validates multiboot magic number
   - Sets up 32KB kernel stack
   - Calls `kmain(magic, multiboot_info)`

3. **kernel.c:kmain()** - Main initialization
   - VGA text mode init
   - GDT setup (Ring 0/3 segments + TSS)
   - IDT setup (256 interrupt handlers)
   - PIC remapping (IRQ 0-15 → INT 32-47)
   - **VBE framebuffer detection and mapping**
   - PMM initialization from multiboot memory map
   - VMM initialization (paging enabled)
   - **VBE framebuffer mapped into virtual memory**
   - Timer, keyboard, mouse drivers
   - PCI enumeration
   - VirtIO device detection
   - Network stack initialization
   - Syscall handler registration
   - Process manager and scheduler
   - RamFS and VFS initialization
   - **GUI subsystem initialization**
   - Shell event loop

### Key Files

**Boot & Core:**
- `boot.asm` - Multiboot entry, GDT definition
- `kernel.c` - Main entry point, initialization, shell
- `linker.ld` - Memory layout and sections

**CPU & Memory:**
- `gdt.c` - Global Descriptor Table and TSS
- `idt.c` - Interrupt Descriptor Table
- `isr.asm` - Interrupt service routine stubs
- `pmm.c/h` - Physical memory manager
- `vmm.c/h` - Virtual memory manager (paging)

**Process & Threading:**
- `process.c/h` - Process table and management
- `scheduler.c/h` - Round-robin scheduler
- `thread.c/h` - Threading support
- `switch.asm` - Context switching
- `exec.c` - Process execution
- `elf.c/h` - ELF executable loader

**System Calls:**
- `syscall.c` - INT 0x80 handler
- `dosapi.c/h` - OS/2 DOS API implementation
- `linux_syscall.c/h` - Linux compatibility layer

**IPC & Synchronization:**
- `semaphore.c/h` - Semaphores (mutex, event)
- `ipc.c/h` - Pipes, queues, shared memory

**File Systems:**
- `ramfs.c/h` - In-memory filesystem
- `vfs.c/h` - Virtual filesystem layer
- `ramfs_vfs.c` - VFS wrapper for RamFS
- `dos_file.c/h` - DOS-style file operations

**Device Drivers:**
- `keyboard.c` - PS/2 keyboard (IRQ 1)
- `mouse.c` - PS/2 mouse (IRQ 12)
- `timer.c` - PIT timer (IRQ 0)
- `rtc.c/h` - Real-time clock
- `serial.c/h` - Serial ports (COM1/COM2)
- `parallel.c/h` - Parallel port
- `speaker.c/h` - PC speaker
- `ide.c/h` - IDE/ATA disk controller
- `pci.c/h` - PCI bus enumeration
- `virtio.c/h` - VirtIO framework
- `virtio_net.c/h` - VirtIO network driver

**Graphics & GUI:**
- `vga.c/h` - VGA text mode
- `graphics.c` - Basic graphics primitives (Mode 13h)
- `vga_gfx.c/h` - VGA graphics subsystem with VBE support
- `vbe.c/h` - VESA BIOS Extensions (high-res framebuffer)
- `gui.c/h` - Window manager (windows, taskbar, cursor)

**Networking:**
- `net.c/h` - TCP/IP stack (Ethernet, IP, UDP, TCP)

**Utilities:**
- `printf.c` - Kernel printf implementation
- `io.asm` - Port I/O functions (inb, outb, etc.)
- `events.c/h` - Event queue (keyboard, mouse)
- `types.h` - Type definitions (uint32_t, etc.)

**Testing:**
- `test_proc.c` - Process management tests
- `usermode.c/h` - User mode transition tests
- `userprogs/` - User mode test programs (hello.asm, loop.asm)

---

## OS/2 Compatibility Goals

### Target Compatibility

**Binary Compatibility:**
- Load and execute LX (Linear eXecutable) format files
- Support OS/2 DLLs (DOSCALLS.DLL, VIOCALLS.DLL, etc.)
- Import table resolution and dynamic linking
- Fixup processing for relocations

**API Compatibility:**
- **DOS API** (DosXXX functions) - ✅ Partially implemented
- **VIO API** (VioXXX functions - text mode) - 📋 Planned
- **KBD API** (KbdXXX functions - keyboard) - 📋 Planned
- **MOU API** (MouXXX functions - mouse) - 📋 Planned
- **Presentation Manager** (PMWIN.DLL - GUI) - 📋 Planned

**Feature Compatibility:**
- ✅ Multi-threading (implemented)
- ✅ Named pipes (implemented)
- ✅ Semaphores (mutex, event, muxwait) - implemented
- ✅ Message queues (implemented)
- 📋 REXX scripting (planned)
- 📋 CONFIG.SYS parsing (planned)

### Non-Goals

- SOM/DSOM compatibility (too complex for initial versions)
- Multiple architecture support (x86 32-bit only)
- Full Workplace Shell (basic version planned)
- OS/2 kernel-level internals (we use modern design)

---

## Development

### Project Structure

```
OSTWO_2026/
├── boot.asm               - Multiboot bootloader entry point
├── kernel.c               - Main kernel (kmain, shell, event loop)
├── io.asm                 - Low-level I/O (inb, outb, etc.)
├── isr.asm                - Interrupt service routines
├── switch.asm             - Context switching

├── gdt.c                  - Global Descriptor Table
├── idt.c                  - Interrupt Descriptor Table
├── pmm.c/h                - Physical memory manager
├── vmm.c/h                - Virtual memory manager

├── process.c/h            - Process management
├── scheduler.c/h          - Round-robin scheduler
├── thread.c/h             - Threading support
├── exec.c                 - Process execution
├── elf.c/h                - ELF loader

├── syscall.c              - System call handler
├── dosapi.c/h             - DOS API implementation
├── linux_syscall.c/h      - Linux syscall compatibility

├── semaphore.c/h          - Semaphores
├── ipc.c/h                - Pipes, queues, shared memory

├── vfs.c/h                - Virtual filesystem
├── ramfs.c/h              - RamFS implementation
├── ramfs_vfs.c            - VFS wrapper for RamFS
├── dos_file.c/h           - DOS file I/O layer

├── timer.c                - PIT timer driver
├── keyboard.c             - PS/2 keyboard driver
├── mouse.c                - PS/2 mouse driver
├── serial.c/h             - Serial port driver
├── parallel.c/h           - Parallel port driver
├── rtc.c/h                - Real-time clock
├── speaker.c/h            - PC speaker
├── ide.c/h                - IDE disk controller
├── pci.c/h                - PCI bus enumeration
├── virtio.c/h             - VirtIO framework
├── virtio_net.c/h         - VirtIO network driver

├── vga.c/h                - VGA text mode
├── graphics.c             - Graphics primitives (Mode 13h)
├── vga_gfx.c/h            - VGA graphics with VBE support
├── vbe.c/h                - VESA BIOS Extensions
├── gui.c/h                - GUI window manager

├── net.c/h                - TCP/IP network stack

├── printf.c               - Kernel printf
├── events.c/h             - Event queue
├── types.h                - Type definitions
├── usermode.c/h           - User mode tests

├── test_proc.c            - Process tests
├── userprogs/             - User mode programs
│   ├── hello.asm
│   ├── loop.asm
│   └── Makefile

├── linker.ld              - Linker script
├── Makefile               - Build system

├── README.md              - This file
├── CLAUDE.md              - Development guide
├── OSTWO_ROADMAP.md       - Development roadmap
├── PHASE1_COMPLETE.md     - Phase 1 milestone report
├── PHASE2_COMPLETE.md     - Phase 2 milestone report
└── *.md                   - Various documentation files
```

### Coding Standards

- **Language:** C99 and x86 assembly (NASM syntax)
- **Style:** K&R style, 4-space indentation, no tabs
- **Naming:**
  - OS/2 API: `DosXXX()`, `VioXXX()`, `KbdXXX()`, etc.
  - Internal kernel: `lowercase_with_underscores()`
  - Macros/constants: `UPPER_CASE_WITH_UNDERSCORES`
- **Comments:**
  - Document all public APIs with function headers
  - Use `// File: filename.c` at top of each file
  - Inline comments for complex logic
- **Error Handling:**
  - Return codes follow OS/2 conventions (0 = success)
  - Use uint32_t for return codes
  - NULL for invalid pointers

### Build System

The Makefile supports multiple targets:

- `make` or `make all` - Build kernel.bin
- `make clean` - Remove all build artifacts
- `make iso` - Create bootable ISO with GRUB
- `make run` - Run in QEMU (direct kernel boot)
- `make run-iso` - Run from ISO with VBE graphics ⭐ **Recommended**
- `make run-hires` - Run with VBE high-resolution
- `make run-virtio-gpu` - Run with VirtIO GPU
- `make run-curses` - Run in curses mode
- `make run-nographic` - Run without display (serial only)
- `make debug` - Start QEMU with GDB server
- `make info` - Show build configuration

### Contributing

Contributions welcome! Please:
1. Follow the coding standards above
2. Test on QEMU before submitting (use `make run-iso`)
3. Document new APIs and features
4. Update this README.md if adding major features
5. Add tests for new functionality

**Areas needing contribution:**
- LX executable loader
- FAT filesystem driver
- Full TCP/IP stack
- Presentation Manager GUI
- DOS INT 21h compatibility
- Device drivers (AHCI, USB, etc.)

---

## Roadmap Highlights

### Phase 1: Foundation ✅ COMPLETE
- ✅ Basic DOS API (DosWrite, DosExit, etc.)
- ✅ Process management with scheduler
- ✅ System calls (INT 0x80)
- ✅ User mode (Ring 3) support
- ✅ Virtual memory with paging

### Phase 2: Advanced Process Management ✅ COMPLETE
- ✅ Multi-threading support
- ✅ Thread creation APIs (DosCreateThread)
- ✅ Semaphores (mutex, event, muxwait)
- ✅ Named pipes and message queues
- ✅ Parent-child process hierarchy
- ✅ Memory allocation APIs (DosAllocMem, DosFreeMem)

### Phase 3: Graphics & GUI 🚧 IN PROGRESS
- ✅ VBE high-resolution support (1024x768x32)
- ✅ GUI window manager
- ✅ Mouse cursor with proper rendering
- 🚧 Full Presentation Manager API
- 📋 Window controls (buttons, text boxes, menus)
- 📋 Bitmap and icon support

### Phase 4: File Systems 📋 PLANNED
- 📋 VFS layer enhancement
- 📋 FAT16/FAT32 support
- 📋 HPFS (High Performance File System)
- 📋 Full DOS file API
- 📋 Disk persistence

### Phase 5: LX Executable Loader 📋 PLANNED
- 📋 Parse LX file format
- 📋 Load OS/2 executables
- 📋 DLL support and dynamic linking
- 📋 Import table resolution
- 📋 Run real OS/2 applications

### Phase 6: DOS Compatibility Box 📋 PLANNED
- 📋 INT 21h handler
- 📋 DOS memory management
- 📋 Run DOS programs
- 📋 DOS device drivers

### Phase 7-10: Advanced Features 📋 PLANNED
- Network stack completion (full TCP/IP)
- Sound card drivers
- USB support
- REXX scripting language
- Workplace Shell clone
- SOM (System Object Model)

**See [OSTWO_ROADMAP.md](OSTWO_ROADMAP.md) for complete timeline and details.**

---

## Documentation

### In This Repository

- **[README.md](README.md)** - This file (overview and quick start)
- **[CLAUDE.md](CLAUDE.md)** - Detailed development guide and architecture
- **[OSTWO_ROADMAP.md](OSTWO_ROADMAP.md)** - Complete development roadmap
- **[PHASE1_COMPLETE.md](PHASE1_COMPLETE.md)** - Phase 1 milestone report
- **[PHASE2_COMPLETE.md](PHASE2_COMPLETE.md)** - Phase 2 milestone report
- **[SYSCALL_SUCCESS_REPORT.md](SYSCALL_SUCCESS_REPORT.md)** - Syscall implementation
- **[USER_MODE_TEST_RESULTS.md](USER_MODE_TEST_RESULTS.md)** - User mode testing
- **[TESTING_GUIDE.md](TESTING_GUIDE.md)** - Testing procedures

### External Resources

- **OS/2 API Reference:** https://archive.org/details/os2apiref
- **LX Format Specification:** http://www.edm2.com/index.php/LX
- **OS/2 Museum:** http://www.os2museum.com/
- **EDM/2:** http://www.edm2.com/ (OS/2 developer resources)
- **VBE 3.0 Specification:** https://www.vesa.org/ (VESA standards)
- **OSDev Wiki:** https://wiki.osdev.org/ (OS development resources)

---

## Why OS/Two?

### Why build an OS/2 clone in 2026?

1. **Nostalgia:** OS/2 was technically superior to Windows 95/98 in many ways
2. **Learning:** Building an OS from scratch teaches low-level programming
3. **Preservation:** Keep OS/2 software and knowledge accessible
4. **Challenge:** It's a fascinating engineering problem
5. **Legacy:** Some industries still use OS/2 (ATMs, point-of-sale, industrial)
6. **Modern Take:** Apply modern techniques to a classic design

### Why not just use ArcaOS?

**ArcaOS** (commercial OS/2 continuation by Arca Noae) is excellent for running real OS/2 on modern hardware. **OS/Two** is different:

- **Open Source** - Full source code availability under permissive license
- **Educational** - Designed for learning and experimentation
- **Modern Implementation** - Clean codebase using modern techniques
- **Experimental** - Can try new ideas without strict compatibility constraints
- **Transparent** - All code visible and modifiable

We're not competing with ArcaOS; we're preserving OS/2's concepts in an open, educational project.

---

## Screenshots

*(Screenshots would go here showing GUI, shell commands, etc.)*

### Text Mode Shell
```
OS/Two v0.3 (Alpha) - OS/2-Compatible Operating System
Boot time: 2026-01-11 12:34:56

kernel> help
Available commands:
  help, clear, info, mem, vmm, ps, ls, cat, touch, write, rm
  testproc, testsyscall, usermode, gui, desktop, netinfo
  date, time, reboot, halt, beep

kernel> mem
Physical Memory Manager Status:
  Total memory: 64 MB
  Used: 8192 KB (12.5%)
  Free: 57344 KB (87.5%)

kernel> ps
PID  PPID State    Priority Name
  0    -1 RUNNING  10       [kernel]
  1     0 READY    5        idle
```

### GUI Mode (1024x768)
- Desktop with taskbar
- Draggable windows
- Start Menu
- Smooth mouse cursor

---

## Technical Highlights

### What Makes OS/Two Unique?

1. **True OS/2 API Compatibility**
   - Implements actual DOS API functions (DosXXX)
   - Binary-level syscall compatibility
   - Plans for LX executable support

2. **Modern Architecture**
   - Clean separation of kernel/user mode
   - Modular design with VFS, device drivers
   - Well-documented codebase

3. **High-Resolution Graphics**
   - VBE support for 1024x768x32
   - 32-bit color handling
   - Hardware framebuffer acceleration

4. **Full Networking Stack**
   - TCP/IP implementation
   - VirtIO network driver
   - Ethernet, IP, UDP, TCP protocols

5. **Multi-Threading & IPC**
   - Preemptive multitasking
   - Semaphores, pipes, message queues
   - Priority scheduling

---

## Known Issues

### Current Limitations

- **File Systems:** Only RamFS (in-memory), no disk persistence yet
- **Executables:** Only ELF format, LX loader not implemented
- **Networking:** Basic stack, not production-ready
- **GUI:** Window manager functional, but limited controls
- **Hardware:** Limited driver support (no USB, AHCI, etc.)
- **Memory:** 64MB limit due to PMM bitmap size

### Bug Fixes in This Version

- ✅ **Fixed:** Mouse cursor trails in 32-bit color mode
  - Changed cursor save buffer from uint8_t to uint32_t
  - Implemented vga_get_pixel32() and vga_plot_pixel32()
  - Proper save/restore of full RGB values

---

## FAQ

**Q: Can it run OS/2 Warp applications?**
A: Not yet. The LX executable loader is planned for Phase 5. Currently it can run custom ELF programs and built-in tests.

**Q: Will it have a GUI?**
A: Yes! A basic GUI window manager is already implemented (1024x768 with VBE). Full Presentation Manager clone is planned for later phases.

**Q: Can it run DOS programs?**
A: Not yet. DOS compatibility box (INT 21h handler) is planned for Phase 6.

**Q: What's the performance like?**
A: Early stage - focus is on correctness and features, not optimization. Performance tuning will come later.

**Q: Does it run on real hardware?**
A: Theoretically yes (it's multiboot-compliant), but only tested on QEMU. Real hardware testing needs more driver support.

**Q: Can I help develop it?**
A: Yes! Contributions are welcome. See the Contributing section above.

**Q: What license will you use?**
A: Likely MIT or BSD (permissive). Final decision pending.

**Q: How much of OS/2 is implemented?**
A: Basic DOS API (~30%), no VIO/KBD/MOU APIs yet, no LX loader, no PM GUI yet. This is early alpha.

**Q: Will it be compatible with OS/2 device drivers?**
A: No. OS/2 drivers are kernel-mode and very platform-specific. We'll write new drivers with similar APIs.

---

## Performance

### System Requirements

**Minimum:**
- i386 CPU (32-bit x86)
- 64 MB RAM
- VGA graphics
- PS/2 keyboard/mouse

**Recommended:**
- i686 CPU or better
- 128 MB RAM
- VESA-compatible graphics card
- QEMU or VirtualBox for testing

### Boot Time

- GRUB → Kernel init: ~2 seconds (on QEMU)
- Kernel init → Shell prompt: ~1 second
- Total boot: ~3 seconds

### Memory Usage

- Kernel size: ~164 KB (kernel.bin)
- ISO size: ~30 MB (with GRUB)
- Runtime RAM: ~8-12 MB kernel + processes
- Supports up to 256 MB physical RAM (PMM bitmap limit)

---

## Credits

**OS/Two** is inspired by and built upon the knowledge from:

- **IBM OS/2** - The original operating system that inspired this project
- **Minix** - Educational OS design principles
- **xv6** - Clean, understandable teaching OS
- **Linux** - Modern kernel techniques and device drivers
- **SerenityOS** - Modern OS development practices

**Developed with assistance from:**
- **Claude** (Anthropic) - AI programming assistant for code generation and debugging
- **The open-source community** - OSDev wiki, forums, and resources

**Special Thanks:**
- OS/2 Museum for preservation efforts
- EDM/2 for OS/2 documentation
- QEMU project for excellent emulation

---

## License

**To be determined** (likely MIT or BSD)

Copyright (c) 2026 OS/Two Project

*Provisional license: Open source, free to use, modify, and distribute.*

---

## Contact & Community

- **Repository:** https://github.com/[TBD]/ostwo (GitHub link TBD)
- **Website:** http://ostwo.org or http://os-two.org (TBD)
- **Discord:** [Discord server link TBD]
- **Email:** ostwo-dev@[TBD]
- **Issue Tracker:** Use GitHub Issues

**Get Involved:**
- Report bugs and request features on GitHub
- Join discussions on Discord
- Contribute code via pull requests
- Write documentation and tutorials
- Test on real hardware and report results

---

## Changelog

### Version 0.3 Alpha (Current)
- ✅ Added VBE high-resolution support (1024x768x32)
- ✅ Implemented GUI window manager with taskbar
- ✅ Fixed mouse cursor trails in 32-bit color mode
- ✅ Added VirtIO network driver
- ✅ Implemented basic TCP/IP stack
- ✅ Added threading and IPC (semaphores, pipes)
- ✅ Enhanced shell with 40+ commands
- ✅ Added RTC support for date/time
- ✅ Implemented ELF executable loader
- ✅ Added PCI bus enumeration

### Version 0.2 Alpha
- ✅ Multi-process support
- ✅ Round-robin scheduler
- ✅ DOS API syscalls
- ✅ User mode (Ring 3) execution
- ✅ Process creation and management

### Version 0.1 Alpha
- ✅ Basic kernel with GDT/IDT
- ✅ Memory management (PMM/VMM)
- ✅ VGA text mode
- ✅ Keyboard driver
- ✅ Interactive shell

---

## Building from Source (Detailed)

### Step-by-Step Build Instructions

1. **Clone the repository** (when available):
   ```bash
   git clone https://github.com/[TBD]/ostwo.git
   cd ostwo
   ```

2. **Install dependencies** (see Prerequisites section above)

3. **Build the kernel**:
   ```bash
   make clean   # Remove old build artifacts
   make         # Compile kernel.bin
   ```

4. **Create bootable ISO**:
   ```bash
   make iso     # Creates ostwo.iso with GRUB
   ```

5. **Run in QEMU**:
   ```bash
   make run-iso # Boot from ISO (recommended)
   ```

### Troubleshooting Build Issues

**Error: "gcc: command not found"**
- Install gcc: `sudo apt install gcc` (Debian/Ubuntu)

**Error: "-m32: not supported"**
- Install 32-bit libraries: `sudo apt install gcc-multilib`

**Error: "nasm: command not found"**
- Install nasm: `sudo apt install nasm`

**Error: "grub-mkrescue: command not found"**
- Install grub tools: `sudo apt install grub2-common xorriso`

**Linker errors about "elf_i386"**
- You need 32-bit linker support. On 64-bit systems:
  - Fedora: `sudo dnf install glibc-devel.i686`
  - Debian/Ubuntu: Already included in gcc-multilib

---

**OS/Two - Where Classic Meets Modern!**

*The open-source OS/2-compatible operating system for the 21st century.*

---

**Last Updated:** 2026-01-11
**Version:** 0.3 Alpha
**Build:** 164120 bytes (kernel.bin)
