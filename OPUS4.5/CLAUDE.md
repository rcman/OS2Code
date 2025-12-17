# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an educational OS/2-style operating system that boots in QEMU. It's a minimal x86 operating system written in C++ and assembly that demonstrates bootloading, protected mode transitions, and basic hardware drivers.

## Build Commands

All build commands must be run from the `build/` directory:

```bash
cd build
chmod +x build.sh run.sh  # First time only
./build.sh                 # Build the OS
./run.sh                   # Run in QEMU
```

Manual QEMU invocation:
```bash
qemu-system-i386 -drive file=build/os2clone.img,format=raw,if=floppy -boot a -m 16M
```

## Architecture

### Boot Sequence
1. BIOS loads `boot/boot.asm` (512-byte bootloader) from first sector
2. Bootloader loads kernel from disk sectors into 0x1000
3. Bootloader sets up GDT and switches to 32-bit protected mode
4. Jumps to `kernel/entry.asm` at 0x1000
5. Entry point sets up stack and calls `kernel_main()` in `kernel/kernel.cpp`
6. Kernel initializes VGA driver, keyboard driver, and shell
7. Shell runs command loop

### Memory Layout
- `0x0000 - 0x0FFF`: Real mode IVT and BDA
- `0x1000 - 0x7BFF`: Kernel code and data
- `0x7C00 - 0x7DFF`: Bootloader (512 bytes)
- `0x7E00+`: Stack (grows downward)
- `0xB8000`: VGA text buffer (memory-mapped I/O)

### Component Architecture

**Boot Layer** (`boot/boot.asm`):
- 16-bit real mode bootloader
- Disk loading via BIOS INT 13h
- GDT setup and protected mode transition
- Must be exactly 512 bytes (sector 0)

**Kernel Layer** (`kernel/`):
- `entry.asm`: Assembly entry point, sets up stack, calls C++
- `kernel.cpp`: Main kernel, initializes drivers and shell
- `string.cpp/h`: Freestanding string utilities (no libc)
- `types.h`: Type definitions for freestanding environment

**Driver Layer** (`drivers/`):
- `vga.cpp/h`: VGA text mode (80x25), direct framebuffer writes to 0xB8000
- `keyboard.cpp/h`: PS/2 keyboard via port I/O (polling, no interrupts)

**Shell Layer** (`shell/`):
- `shell.cpp/h`: Command interpreter with OS/2-style commands
- Commands: HELP, CLS, VER, ECHO, DIR, TYPE, CD, DATE, TIME, MEM, SET, COLOR, SYSINFO, EXIT

### Build System

**Compiler Flags** (from `build.sh`):
```bash
CXXFLAGS="-m32 -ffreestanding -fno-exceptions -fno-rtti -nostdlib -fno-pie -fno-stack-protector -mno-red-zone -Wall -Wextra -O2"
```
- `-m32`: Generate 32-bit i386 code
- `-ffreestanding`: No hosted environment (no OS)
- `-fno-exceptions -fno-rtti`: Disable C++ features requiring runtime support
- `-nostdlib`: No standard library
- `-fno-pie -fno-stack-protector`: Disable security features that require runtime
- `-mno-red-zone`: Required for kernel code (AMD64 ABI compatibility)

**Linker Script** (`build/linker.ld`):
- Entry point: `_start` (from `kernel/entry.asm`)
- Base address: 0x1000
- Generates flat binary (no ELF headers)

**Disk Image**:
- 1.44MB floppy image (2880 sectors)
- Sector 1: Bootloader (512 bytes)
- Sector 2+: Kernel binary

### Code Conventions

**Include Paths**: All includes use paths relative to repository root:
```cpp
#include "../kernel/types.h"
#include "../drivers/vga.h"
```

**Global Objects**: Drivers and shell use extern global instances:
```cpp
extern VGA vga;
extern Keyboard keyboard;
extern Shell shell;
```

**Freestanding Environment**:
- No `printf`, `malloc`, `new`, `delete`
- No exceptions or RTTI
- Custom string functions in `kernel/string.cpp`
- Direct hardware access via pointers and port I/O

**I/O Operations**:
- VGA: Direct writes to `(uint16_t*)0xB8000`
- Keyboard: Read from port 0x60 using inline assembly
- No interrupt handling (polling-based)

## Development Notes

**Debugging**:
- Use QEMU monitor (Ctrl+Alt+2 in SDL mode)
- Run with `-d int,cpu_reset` for debug output
- Check bootloader: `xxd build/boot.bin | head` (should end with 55 AA)

**Limitations**:
- No real filesystem (directory commands are simulated)
- No multitasking or process management
- No interrupts (keyboard polling only)
- No dynamic memory allocation
- Stack is 16KB (defined in `kernel/entry.asm`)

**Adding New Commands**:
1. Declare handler in `shell/shell.h`
2. Implement in `shell/shell.cpp`
3. Add to command table in `executeCommand()`

**Adding New Drivers**:
1. Create header/implementation in `drivers/`
2. Include in `kernel/kernel.cpp`
3. Add compilation to `build/build.sh`
4. Add object file to linker command

**Testing Changes**:
Always rebuild completely after changes:
```bash
cd build
./build.sh && ./run.sh
```

**Important Implementation Details**:
- The bootloader reads the kernel in two batches (17 + 20 sectors) to avoid BIOS INT 13h limitations when reading across track boundaries
- Keyboard driver uses busy-waiting (polling) instead of `hlt` instruction since no interrupt handlers are configured
- VGA output requires a graphical QEMU display (SDL/GTK); `-display curses` or `-nographic` won't show kernel output
