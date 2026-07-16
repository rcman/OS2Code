# OS/Two — Bug Fixes and Improvements

All changes were verified by booting the kernel in QEMU (both `-kernel`
mode-13h boot and the GRUB ISO / VBE 1024x768 boot), driving the shell
through the QEMU monitor, and checking screenshots + serial logs.

## Critical fixes

### 1. Kernel crash (triple fault) after any `exec` — exec.c, switch.asm
`run_process_and_wait()` used a GCC computed-goto label (`&&return_point`)
plus an inline-asm `jmp` to return from a user process. At `-O2` the
compiler placed the label at the *top* of the function and kept live
values in callee-saved registers, so "returning" re-entered the launch
code with a corrupted `%ebx`: the kernel page directory address was
dereferenced as a `process_t*`, `state=RUNNING` was written into
`kernel_page_directory[1]`, and `vmm_switch_page_directory()` was called
with 0 — loading **CR3=0** and triple-faulting the machine.

Fixed with proper setjmp/longjmp-style context helpers in assembly
(`kctx_save` / `kctx_restore` in switch.asm) that explicitly save and
restore all callee-saved registers, ESP and EIP. Safe at any
optimization level.

### 2. Shell froze after a process exited — exec.c
The EXIT syscall enters through an interrupt gate, so IF is clear when
the kernel context is resumed. Nothing re-enabled interrupts, so the
event loop `hlt`-ed forever with the timer and keyboard dead. Now `sti`
is executed on resume.

### 3. `vmm_destroy_page_directory` freed the kernel's own page table — vmm.c
The destroy loop started at PDE 0, but PDE 0 is the identity-mapping
page table *shared* with the kernel page directory. Destroying any
process page directory would have freed the kernel's first 4 MB page
table and every physical page it maps. The loop now skips all shared
identity-map PDEs.

### 4. Physical memory above 4 MB was unusable — vmm.c, pmm.c
Only the first 4 MB were identity-mapped, but the kernel accesses page
directories, page tables, process code pages and kernel stacks through
their *physical* addresses, and the PMM allocates lowest-first from
1 MB upward. As soon as cumulative allocations crossed the 4 MB line,
every such access would page-fault. `vmm_init()` now identity-maps all
detected RAM (`pmm_get_highest_address()`, new API), and process page
directories share those mappings.

### 5. User space moved to 0x40000000 — process.c, userprogs/
User binaries loaded at 0x400000, which now falls inside the shared
identity map — mapping user pages there would silently rewrite the
kernel's (and every process's) view of physical memory. Programs now
load at 0x40000000, `vmm_map_page()` refuses user mappings inside the
identity region (clean error instead of silent corruption), and
hello.bin / loop.bin were rebuilt for the new org.

### 6. Process resources leaked on every exec — process.c, exec.c
Terminated processes were "cleaned after context switch" — which never
happened on the exec path, leaking the page directory, its page tables,
user pages, the kernel stack and the PCB slot every run. New
`process_reap_terminated()` frees all of it after each exec returns.

## Functional fixes

### 7. printf ignored width/length modifiers — printf.c
`%02x`, `%04x`, `%8d`, `%llu` etc. printed the literal format text, so
driver logs were unreadable ("MAC: %02x:%02x...") and 64-bit arguments
left the va_list misaligned. Rewritten with flags (`-`, `0`), field
width, and `l`/`ll` length support; 64-bit division helpers come from
libgcc, which the kernel now links (`Makefile`). VirtIO capacity and
MAC/PCI logs now print correctly.

### 8. htons/htonl evaluated arguments multiple times — net.h
They were macros, so `htons(ip_id_counter++)` (net.c) incremented the
counter 2–3 times per packet and produced wrong IP IDs — undefined
behavior flagged by `-Wsequence-point`. Replaced with inline functions.

### 9. Start button / taskbar clicks never worked in VGA mode — gui.c, mouse.c
GUI hit-testing, menu placement, clipping and mouse clamping hardcoded
1024x768 while the default boot runs at 320x200 — the taskbar was
hit-tested at y>=738 on a 200-line screen, so clicks could never land.
All of it now reads the active mode via `vga_get_mode_info()` (which
also gained safe defaults before the first mode set). Verified by
opening the Start menu with monitor-injected mouse clicks.

### 10. User program output was invisible in graphics mode — syscall.c, printf.c, kernel.c
The WRITE syscall wrote only to the VGA *text* buffer, which isn't
displayed once the GUI is up, and kernel printf output likewise never
reached the screen. printf now supports an optional GUI sink installed
by the shell, so exec status, ps/mem output and user programs' own
DosWrite output appear in the shell window (and still go to serial).

## Minor fixes

- **ide.c**: ATAPI devices (CD-ROMs) answer ATA IDENTIFY with an abort
  + signature; this was reported as a scary DRQ error. The signature is
  now detected and reported as "ATAPI device, skipping".
- **syscall.c**: the `[Syscall-Debug]` lines printed on every program
  run are now behind `#ifdef SYSCALL_DEBUG`.
- **pmm.c**: `total_pages` was computed from the *sum* of region sizes,
  but the bitmap is indexed by absolute page frame number — memory-map
  holes made the top pages unusable. Now based on the highest usable
  address.
- **Makefile**: links libgcc (64-bit arithmetic helpers); documents and
  silences the intentional single-RWX-segment linker warning.
- ***.asm**: added `.note.GNU-stack` sections (removes executable-stack
  linker warnings).
