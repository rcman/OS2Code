# OS/Two — source tree

This directory is the live OS/Two source. For the project overview see the
[repository README](../README.md); for the full development history see
[`OSTWO_ROADMAP.md`](OSTWO_ROADMAP.md).

![OS/Two Workplace Shell desktop](docs/ostwo_desktop.png)

## Build & run

**32-bit kernel** (the OS/2-compatible desktop OS):
```bash
make            # build kernel.bin
make run        # boot in QEMU (qemu-system-i386)
make iso        # optional: bootable ISO (needs grub + xorriso)
```

**64-bit kernel** (long mode, ELF64 apps, OS/2 compatibility mode):
```bash
make kernel64   # build ostwo64.bin (needs qemu-system-x86_64)
make run64sh    # interactive shell — type 'gui' for the desktop, 'smp' for the SMP demo
make run64      # native ELF64 app + an OS/2 LX app (compat mode), one boot
make run64mt    # preemptive multitasking demo
```

**Applications**: see [`sdk/`](sdk/) — write C apps with the OS/2 API and build
them into LX executables that run on both kernels.

## What's here

| Area | Files |
|------|-------|
| 32-bit boot & core | `boot.asm`, `kernel.c`, `gdt.c`, `idt.c`, `isr.asm`, `pmm.c`, `vmm.c` |
| Processes / threads / IPC | `process.c`, `scheduler.c`, `thread.c`, `semaphore.c`, `ipc.c` |
| OS/2 LX loader + API | `lx.c`, `lx_thunks.asm`, `dos_file.c`, `syscall.c`, `dosapi.h` |
| Filesystems | `vfs.c`, `ramfs.c`, `ramfs_vfs.c` |
| Workplace Shell desktop | `gui.c`, `vga_gfx.c`, `vbe.c`, `bga.c` |
| Drivers | `keyboard.c`, `mouse.c`, `timer.c`, `rtc.c`, `serial.c`, `pci.c`, `ide.c`, `virtio*.c`, `net.c` |
| 64-bit kernel | `boot64.asm`, `entry64.asm`, `kmain64.c`, `isr64.asm` |
| 64-bit memory + loaders | `pmm64.c`, `vmm64.c`, `elf64.c`, `lx64.c`, `thunk32.asm` |
| 64-bit scheduler + drivers | `sched64.c`, `tasks64.asm`, `kbd64.c`, `mouse64.c` |
| SDK | `sdk/` |

## Coding notes

- C99 and NASM assembly; K&R style, 4-space indentation.
- OS/2 API names are `DosXxx()`; internal kernel functions are
  `lowercase_with_underscores()`.
- Return codes follow OS/2 conventions (0 = success).

Historical milestone reports (`PHASE*_COMPLETE.md`, `*_SUCCESS_REPORT.md`, etc.)
are kept in this directory for reference.
