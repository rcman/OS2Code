# OS/Two - OS/2-Compatible Operating System
## Development Roadmap

**Project Vision:** Create a modern, open-source operating system compatible with OS/2 APIs and executables.

**Name:** OS/Two (pronounced "OS Two" - a clever homage to OS/2)

**Current Version:** 0.3 (formerly SimpleOS v0.3)

---

## Phase 1: Foundation & Rebranding ✅ COMPLETE

### 1.1 Branding ✅ COMPLETE
- [x] System call interface (DosWrite, DosExit, DosGetPID)
- [x] Rename "SimpleOS" → "OS/Two" in all files
- [x] Update boot banner
- [ ] Create new logo/branding (optional)
- [x] Update Makefile and build system

### 1.2 Core Security ✅ COMPLETE
- [x] Set IOPL=0 for proper privilege separation
- [x] Test syscall security enforcement
- [x] Implement GPF handler for invalid I/O

### 1.3 Complete Basic DOS API ✅ COMPLETE
- [x] DosWrite (fd, buffer, length)
- [x] DosExit (exitcode)
- [x] DosGetPID ()
- [x] DosExecPgm - Execute program from file (BONUS!)
- [x] DosRead (fd, buffer, length) - Stub for Phase 2
- [x] DosSleep (milliseconds) - Busy-wait implementation
- [x] DosBeep (frequency, duration)
- [x] DosGetDateTime (structure)
- [x] DosSetDateTime (structure)

**Target: 2-3 weeks**

---

## Phase 2: Process & Memory Management (OS/2 Style) ✅ COMPLETE

### 2.1 Advanced Process API ✅
- [x] **DosExecPgm** - Execute program from file (BONUS from Phase 1!)
- [ ] **DosWaitChild** - Wait for child process (deferred)
- [x] **DosKillProcess** - Terminate process by PID
- [x] **DosGetPPID** - Get parent process ID
- [ ] **DosSuspendThread** / **DosResumeThread** - Thread control (deferred)
- [x] **DosSetPriority** - Change process/thread priority

### 2.2 Threading Support ✅
OS/2 is multi-threaded. Implemented:
- [x] Thread structure (separate from process)
- [x] **DosCreateThread** - Create new thread
- [ ] **DosWaitThread** - Wait for thread completion (deferred)
- [x] Per-thread stacks (16KB per thread)
- [ ] Thread-local storage (TLS) (deferred)
- [x] Thread infrastructure (scheduler integration pending)

### 2.3 Memory Management API ✅
- [x] **DosAllocMem** - Allocate memory block
- [x] **DosFreeMem** - Free memory block
- [ ] **DosSetMem** - Change memory attributes (deferred)
- [ ] **DosSubAllocMem** / **DosSubFreeMem** - Suballocation (deferred)
- [ ] **DosGiveSharedMem** / **DosGetSharedMem** - Shared memory (deferred)
- [ ] **DosQueryMem** - Query memory attributes (deferred)

### 2.4 Process Synchronization ✅ BONUS!
- [x] **DosCreateMutexSem** - Create mutex semaphore
- [x] **DosCreateEventSem** - Create event semaphore
- [x] **DosSemRequest** - Acquire/wait on semaphore
- [x] **DosSemClear** - Release/clear semaphore
- [x] **DosSemSet** - Signal event semaphore
- [x] **DosCloseSem** - Close semaphore

**Status: COMPLETE ✅**
**Completion Date: December 2025**
**Documentation: See PHASE2_COMPLETE.md**

---

## Phase 3: File System (HPFS-Inspired)

### 3.1 Virtual File System (VFS)
- [ ] Abstract filesystem interface
- [ ] Mount point support
- [ ] Path parsing (/drive/path/file)
- [ ] Current directory per process

### 3.2 OS/2 File API
- [ ] **DosOpen** - Open file (create/truncate/append modes)
- [ ] **DosClose** - Close file handle
- [ ] **DosRead** / **DosWrite** - File I/O
- [ ] **DosSetFilePtr** - Seek in file
- [ ] **DosQueryFileInfo** / **DosSetFileInfo** - File metadata
- [ ] **DosDelete** - Delete file
- [ ] **DosMkDir** / **DosRmDir** - Directory operations
- [ ] **DosFindFirst** / **DosFindNext** / **DosFindClose** - Directory enumeration

### 3.3 Filesystem Implementation
- [ ] Upgrade RamFS to support long filenames
- [ ] Add directory support to RamFS
- [ ] Implement FAT16/FAT32 driver (DOS compatibility)
- [ ] Optional: HPFS-inspired filesystem (long names, extended attributes)

**Target: 2-3 months**

---

## Phase 4: LX Executable Loader (OS/2 Native Format)

### 4.1 LX Format Parser
OS/2 uses LX (Linear eXecutable) format. Need:
- [ ] Parse LX header
- [ ] Load object table (code/data segments)
- [ ] Parse fixup records (relocations)
- [ ] Import table resolution
- [ ] Export table registration

### 4.2 Dynamic Linking
- [ ] DLL loading subsystem
- [ ] Import binding at load time
- [ ] Lazy binding support
- [ ] DLL reference counting

### 4.3 Standard DLLs
Create OS/Two native DLLs:
- [ ] **DOSCALLS.DLL** - DOS API functions
- [ ] **VIOCALLS.DLL** - Video I/O (text mode)
- [ ] **KBDCALLS.DLL** - Keyboard I/O
- [ ] **MOUCALLS.DLL** - Mouse I/O

**Target: 2-4 months**

---

## Phase 5: DOS Compatibility Box

### 5.1 DOS Emulation Layer
- [ ] Virtual 8086 mode support (or emulation)
- [ ] DOS API INT 21h handler
- [ ] DOS memory layout (conventional, extended, expanded)
- [ ] DOS PSP (Program Segment Prefix)
- [ ] DOS environment variables

### 5.2 DOS Executable Loader
- [ ] MZ (DOS .EXE) format loader
- [ ] COM file loader
- [ ] DOS memory allocation
- [ ] DOS file handles

### 5.3 DOS API Implementation
Implement common INT 21h functions:
- [ ] AH=01h - Character input
- [ ] AH=02h - Character output
- [ ] AH=09h - String output
- [ ] AH=3Dh - Open file
- [ ] AH=3Eh - Close file
- [ ] AH=3Fh - Read file
- [ ] AH=40h - Write file
- [ ] AH=4Ch - Exit program

**Target: 3-4 months**

---

## Phase 6: Synchronization & IPC

### 6.1 OS/2 Synchronization Primitives
- [ ] **DosCreateMutexSem** / **DosCloseMutexSem**
- [ ] **DosRequestMutexSem** / **DosReleaseMutexSem**
- [ ] **DosCreateEventSem** / **DosCloseEventSem**
- [ ] **DosPostEventSem** / **DosWaitEventSem** / **DosResetEventSem**
- [ ] **DosCreateMuxWaitSem** - Wait on multiple semaphores

### 6.2 Named Pipes
- [ ] **DosCreateNPipe** - Create named pipe
- [ ] **DosConnectNPipe** - Wait for client connection
- [ ] **DosDisconnectNPipe** - Disconnect client
- [ ] **DosTransactNPipe** - Bidirectional message transaction

### 6.3 Queues
- [ ] **DosCreateQueue** / **DosCloseQueue**
- [ ] **DosWriteQueue** / **DosReadQueue**
- [ ] **DosPurgeQueue** / **DosQueryQueue**

**Target: 1-2 months**

---

## Phase 7: Device Drivers

### 7.1 OS/2 Device Driver Interface
- [ ] Character device drivers
- [ ] Block device drivers
- [ ] Driver registration/unregistration
- [ ] IOCTL interface

### 7.2 Standard Drivers
- [ ] CON (console)
- [ ] PRN (printer - stub)
- [ ] NUL (null device)
- [ ] CLOCK$ (system clock)
- [ ] SCREEN$ (video)
- [ ] KBD$ (keyboard)
- [ ] MOUSE$ (mouse)

### 7.3 Block Device Support
- [ ] Hard disk driver (ATA/IDE)
- [ ] Floppy disk driver
- [ ] CD-ROM driver (optional)

**Target: 2-3 months**

---

## Phase 8: Presentation Manager (PM) Clone

### 8.1 Graphics Foundation
- [ ] VGA/VESA graphics modes
- [ ] Framebuffer abstraction
- [ ] Basic 2D drawing primitives
- [ ] Bitmap support

### 8.2 Window Management
- [ ] Window structure (HWND)
- [ ] Window messages and message queue
- [ ] **WinCreateWindow** / **WinDestroyWindow**
- [ ] **WinShowWindow** / **WinHideWindow**
- [ ] Z-order management

### 8.3 User Input
- [ ] Mouse cursor
- [ ] Keyboard focus
- [ ] Message dispatch (WM_MOUSEMOVE, WM_CHAR, etc.)

### 8.4 Controls
- [ ] Buttons
- [ ] Text entry fields
- [ ] List boxes
- [ ] Scroll bars
- [ ] Menus

### 8.5 GPI (Graphics Programming Interface)
- [ ] Presentation spaces
- [ ] **GpiSetColor** / **GpiSetBackColor**
- [ ] **GpiMove** / **GpiLine** / **GpiBox**
- [ ] **GpiCharString** - Text output
- [ ] Fonts (basic bitmap fonts)

**Target: 6-12 months (massive undertaking)**

---

## Phase 9: Advanced Features

### 9.1 REXX Scripting
- [ ] REXX interpreter
- [ ] REXX API bindings
- [ ] Standard REXX libraries

### 9.2 Configuration Files
- [ ] CONFIG.SYS parser
- [ ] DEVICE= driver loading
- [ ] SET statements (environment)
- [ ] LIBPATH, PATH support

### 9.3 Workplace Shell (WPS) Basics
- [ ] Object-oriented desktop
- [ ] Folder objects
- [ ] Program objects
- [ ] Drag & drop

### 9.4 Networking
- [ ] TCP/IP stack
- [ ] Sockets API
- [ ] NetBIOS support (optional)

**Target: 1+ years**

---

## Phase 10: Polish & Compatibility

### 10.1 Application Compatibility
Test with real OS/2 applications:
- [ ] Command-line utilities
- [ ] Text-mode applications
- [ ] Simple GUI applications
- [ ] Document compatibility issues

### 10.2 Performance Optimization
- [ ] Profiling and benchmarking
- [ ] Optimize critical paths
- [ ] Memory management tuning
- [ ] Scheduler improvements

### 10.3 Documentation
- [ ] API reference documentation
- [ ] Programming guide
- [ ] User manual
- [ ] Migration guide (from OS/2)

**Target: Ongoing**

---

## Technical Architecture

### OS/Two System Architecture

```
┌─────────────────────────────────────────────────────┐
│                  Applications                        │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐          │
│  │ OS/2 APP │  │ DOS APP  │  │ PM APP   │          │
│  │  (LX)    │  │  (MZ)    │  │  (GUI)   │          │
│  └────┬─────┘  └────┬─────┘  └────┬─────┘          │
└───────┼─────────────┼─────────────┼─────────────────┘
        │             │             │
┌───────┼─────────────┼─────────────┼─────────────────┐
│       ▼             ▼             ▼                  │
│  ┌────────────┐ ┌──────────┐ ┌──────────┐          │
│  │ DOSCALLS   │ │ DOS INT  │ │ PMWIN    │          │
│  │   .DLL     │ │   21h    │ │  .DLL    │          │
│  └──────┬─────┘ └────┬─────┘ └────┬─────┘          │
│         │            │            │                  │
│         └────────────┼────────────┘                  │
│                      ▼                               │
│            ┌──────────────────┐                      │
│            │  System Calls    │                      │
│            │   (INT 0x80)     │                      │
│            └────────┬─────────┘                      │
└─────────────────────┼───────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│                  OS/Two Kernel                       │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐          │
│  │ Process  │  │  Memory  │  │   VFS    │          │
│  │ Manager  │  │  Manager │  │          │          │
│  └──────────┘  └──────────┘  └──────────┘          │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐          │
│  │  Thread  │  │   IPC    │  │ Drivers  │          │
│  │Scheduler │  │ Semaphores│  │          │          │
│  └──────────┘  └──────────┘  └──────────┘          │
└─────────────────────────────────────────────────────┘
                      ▼
┌─────────────────────────────────────────────────────┐
│                   Hardware                           │
│     CPU (x86)  │  Memory  │  Disk  │  VGA           │
└─────────────────────────────────────────────────────┘
```

### Key Design Principles

1. **Binary Compatibility**: Load and run OS/2 LX executables
2. **API Compatibility**: Implement OS/2 API functions accurately
3. **Modern Architecture**: Use modern kernel design (not copy OS/2 internals)
4. **Performance**: Optimize for modern hardware
5. **Open Source**: Full source code availability

---

## Comparison: OS/2 vs OS/Two

| Feature                  | OS/2 Warp 4     | OS/Two (Goal)    |
|--------------------------|-----------------|------------------|
| **Executable Format**    | LX              | LX ✅            |
| **DOS Support**          | Yes             | Yes ✅           |
| **32-bit API**           | Yes             | Yes ✅           |
| **Multi-threading**      | Yes             | Yes ✅           |
| **Presentation Manager** | Yes             | Simplified ✅    |
| **REXX**                 | Yes             | Yes ✅           |
| **Workplace Shell**      | Yes             | Basic ✅         |
| **SOM/DSOM**             | Yes             | No ❌            |
| **Installable FS**       | Yes             | Limited ✅       |
| **Architecture**         | x86, PPC        | x86 only         |
| **Source Code**          | Closed          | Open Source ✅   |

---

## Development Priorities

### Must Have (Core Compatibility)
1. ✅ DOS API (DosXXX functions)
2. LX executable loader
3. Threading support
4. File system (FAT + VFS)
5. DOS compatibility box

### Should Have (Enhanced Features)
1. Presentation Manager basics
2. Device drivers
3. Named pipes & IPC
4. REXX scripting

### Nice to Have (Advanced)
1. Full Workplace Shell
2. SOM/DSOM compatibility
3. Networking stack
4. Multiple architecture support

---

## Resources Needed

### Documentation
- **OS/2 API Reference**: https://archive.org/details/os2apiref
- **LX Format Spec**: http://www.edm2.com/index.php/LX_-_Linear_eXecutable_Module_Format_Description
- **OS/2 Programming Guides**: EDM/2 and Hobbes OS/2 Archive

### Reference Systems
- OS/2 Warp 4 (for testing/comparison)
- ArcaOS (modern OS/2 distribution)
- eComStation (commercial OS/2)

### Development Tools
- OS/2 Toolkit
- Watcom C/C++ (OS/2 version)
- LxLite (LX analyzer)

---

## Success Metrics

### Milestone 1: Boot and Run
- [ ] Boot banner says "OS/Two"
- [ ] Run simple LX executable (hello world)
- [ ] List files with "DIR" command

### Milestone 2: DOS Compatibility
- [ ] Run DOS .COM files
- [ ] Run DOS .EXE files
- [ ] DOS utilities work (COMMAND.COM, etc.)

### Milestone 3: Multi-tasking
- [ ] Multiple processes running concurrently
- [ ] Thread creation/synchronization
- [ ] Semaphores working

### Milestone 4: File Operations
- [ ] Read/write files on FAT filesystem
- [ ] Directory operations
- [ ] File attributes and timestamps

### Milestone 5: GUI
- [ ] Display windows
- [ ] Mouse and keyboard input
- [ ] Simple PM application runs

### Milestone 6: Real Application
- [ ] Run a real OS/2 application (e.g., text editor, game)
- [ ] Application fully functional
- [ ] No crashes or major bugs

---

## Community & Ecosystem

### Potential Community
- OS/2 enthusiasts and retro computing fans
- Developers interested in OS development
- Organizations still using OS/2 legacy software

### License
- Recommend: MIT or BSD (permissive open source)
- Allows commercial use and derivatives
- Credit original OS/2 inspiration

### Website/Repository
- GitHub repository: github.com/username/ostw
- Website: ostwo.org or os-two.org
- Documentation wiki
- Forums/Discord for developers

---

## Timeline Estimate

| Phase | Duration | Cumulative |
|-------|----------|------------|
| Phase 1: Foundation | 3 weeks | 3 weeks |
| Phase 2: Process/Memory | 2 months | 3 months |
| Phase 3: File System | 3 months | 6 months |
| Phase 4: LX Loader | 4 months | 10 months |
| Phase 5: DOS Box | 4 months | 14 months |
| Phase 6: IPC | 2 months | 16 months |
| Phase 7: Drivers | 3 months | 19 months |
| Phase 8: PM GUI | 12 months | 31 months |
| Phase 9: Advanced | 12 months | 43 months |
| Phase 10: Polish | Ongoing | - |

**Estimated time to basic OS/2 compatibility: ~14 months**
**Estimated time to GUI (PM): ~31 months (~2.5 years)**

With a small team (2-3 developers), timeline could be reduced significantly.

---

## Immediate Action Items

1. **Rebrand to OS/Two**
   - Update all source files
   - New boot banner
   - Update documentation

2. **Complete Phase 1**
   - Finish basic DOS API
   - Security hardening (IOPL=0)
   - Test suite for syscalls

3. **Start Phase 2**
   - Research threading models
   - Design thread structure
   - Implement DosCreateThread

4. **Documentation**
   - Create API specification document
   - Write developer guide
   - Set up GitHub repository

---

## Phase A: Real OS/2 Application Compatibility (LX) — IN PROGRESS (July 2026)

### A.1 LX Executable Loader ✅ DONE
- [x] LX (Linear Executable) loader: MZ stub + LX header parsing (lx.c/lx.h)
- [x] Object table, object page table, flat and zero-fill pages
- [x] Fixups: internal (0x07 32-bit offset) and self-relative (0x08),
      import-by-ordinal targets, additive values, source lists
- [x] Object rebasing into user arena at 0x40000000 (64KB granularity)
- [x] DOSCALLS import resolution via user-mode thunk page (lx_thunks.asm)
      mapped at 0xB0000000: DosWrite(282), DosExit(234), DosSleep(229),
      DosBeep(286), DosRead(281)
- [x] Format sniffing in shell exec: LX / ELF / flat binary
- [x] Verified end-to-end: hello_os2.exe (real LX file built by
      userprogs/mklx.py) runs, prints via DosWrite, exits via DosExit

### A.2 Toward Unmodified OS/2 Warp Apps — TODO
- [x] Iterated (ITERDATA/ITERDATA2 compressed) page support — both
      decompressors implemented and verified with packed.exe (pages
      compressed by userprogs/mklx.py) ✅
- [x] DosAllocMem(299)/DosFreeMem(304)/DosGetDateTime(230) thunks —
      verified with hello3.exe (Watcom-built) ✅
- [ ] Import-by-name fixups and entry-table (target type 3) fixups
- [ ] 16:32 far pointer and selector fixup sources (0x02/0x05/0x06)
- [ ] DLL loading (KBDCALLS, VIOCALLS, MSG, NLS...) with shared arena
- [x] Run a full C-runtime Watcom program (wcl386 -bt=os2 -l=os2v2):
      hello4.exe with printf (incl. float formatting) runs unmodified ✅
      Added: DosQueryHType(224), DosDevConfig(231), DosResetBuffer(254),
      DosSetFilePtr(256), DosClose(257), DosQueryCp(291),
      DosQuerySysInfo(348, real kernel syscall 51), NLS.6
      DosQueryDBCSEnv; OS/2 process startup frame (module handle,
      environment block, command line on the initial stack); all thunks
      preserve EBX/ESI per _System convention; RamFS files up to 24KB
- [x] File I/O: DosOpen(273)/DosRead/DosWrite/DosClose/DosSetFilePtr
      route to the kernel VFS for handles >= 3 (0-2 stay console);
      C fopen/fprintf/fgets verified end-to-end with hello5.exe ✅
- [x] argc/argv: `exec <prog> [args...]` passes arguments into the
      OS/2 command line; full program path placed between environment
      and command line so argv[0] resolves (OS/2 PIB convention) ✅
- [x] Fixup sources 0x06 (16:32 far, flat selector 0x1B) and
      0x02/0x03 (selector / 16:16 far, null selector - inert without
      LDT tiling); unresolved imports resolve to a warning stub
      instead of failing the load ✅
- [x] Stubs: DosSetRelMaxFH(382), DosQueryModuleHandle(319),
      DosQueryProcAddr(321), DosFlatToSel/DosSelToFlat(425/426) ✅
- [ ] Wider DOSCALLS surface: DosCreateThread, DosLoadModule (DLLs),
      DosGetInfoBlocks (TIB/PIB)...
- [ ] VIO subsystem (VioWrtTTY & friends) + LDT selector tiling for
      16-bit API compatibility
- [x] Test with a genuine Open Watcom-built OS/2 hello world —
      userprogs/hello2.c compiled with wcc386 -bt=os2, linked with
      wlink FORMAT os2 lx (userprogs/build2.sh); runs on OSTwo ✅
- [x] Support EXEs without internal fixups (the wlink default) —
      objects now load at their preferred base: the VMM gives each
      process a private clone of the low identity page table
      (0x10000-0x9FFFF window); modules too big for the window are
      rebased when internal fixups are present ✅
- [x] Hardening found by repeated-exec testing: window content buffers
      were 1 page regardless of size (overflowed at high resolution),
      kernel stacks were 1 page (syscall printf + GUI redraw overflowed
      into adjacent page tables) — both fixed; PMM now refuses to free
      reserved kernel frames ✅
- [ ] Test with real-world binaries (needs DLL loading + wider API)

## Phase B: 64-bit (x86-64 Long Mode) Port — PLANNED

Goal: 64-bit kernel that runs 64-bit ELF apps natively AND keeps running
32-bit OS/2 LX apps in compatibility mode — mirroring how OS/2 2.x ran
16-bit apps on a 32-bit kernel.

- [x] B.0 Boot foundation (July 2026): `make kernel64` builds
      ostwo64.bin — a 32-bit multiboot loader (boot64.asm) that checks
      CPUID long-mode support, identity-maps the first 4GB with 2MB
      pages (PML4/PDPT/4 PDs), enables PAE + EFER.LME, loads a 64-bit
      GDT and jumps to an embedded x86-64 payload (entry64.asm +
      kmain64.c at 2MB). Verified in qemu-system-x86_64 (`make run64`):
      EFER.LMA=1, native 64-bit arithmetic, serial logging, BGA
      1024x768x32 framebuffer driven from 64-bit C ✅
- [x] B.1 Protection + interrupts + userspace (July 2026): 64-bit
      GDT with user segments and TSS (RSP0 interrupt stack), IDT with
      all 32 exception handlers (register dump on serial) + 100 Hz PIT
      timer IRQ, SYSCALL/SYSRET fast path (STAR/LSTAR/SFMASK MSRs),
      user-accessible 2MB page (US bit through the whole walk).
      Verified: a ring-3 64-bit program (isr64.asm blob at 16MB) made
      5 syscalls, computed sum(1..10^7)=0x2D7988896B40 in a 64-bit
      register (bit-exact), timer IRQs served during ring-3 execution,
      clean SYS_EXIT ✅
- [x] B.2 ELF64 applications (July 2026): bitmap PMM (pmm64.c,
      32-256MB), 4KB-granular page mapper above the identity map
      (vmm64.c), ELF64 PT_LOAD loader (elf64.c). Real GCC-compiled
      static ELF64 apps (userprogs/app64.c, linked at 4GB with
      -mcmodel=large) load as multiboot modules (`make run64` passes
      -initrd app64.elf) and run in ring 3. Syscall entry preserves
      all registers except RAX/RCX/R11 (Linux convention). Verified
      bit-exact: F(90), xorshift64 chain, tick timing across a spin ✅
- [x] B.3 32-bit COMPATIBILITY MODE (July 2026) - the capstone
      mechanism: a 32-bit user code segment (CS.L=0, GDT 0x1B) plus a
      DPL-3 int 0x80 gate dispatching the classic OSTwo 32-bit ABI
      (EAX=nr, EBX/ECX/EDX). The 64-bit kernel runs the BYTE-IDENTICAL
      hello.bin that the 32-bit kernel runs (md5 1a7f6903...), in
      ring-3 compat mode, right after the 64-bit ELF app - both in one
      boot. This is the OS/2 model: a 64-bit kernel running 32-bit apps
      in compatibility mode. `make run64` demos both (app64.elf +
      hello.bin as -initrd modules). Key gotcha: 0x40000000 identity-
      maps to physical 1GB which doesn't exist at -m 256M, so the compat
      region is backed by real pmm64 frames (clear PDPT[1] under PML4[0]
      to split the identity huge page, then vmm64_map 4KB pages) ✅
- [x] B.4 OS/2 LX APP UNDER THE 64-BIT KERNEL (July 2026) - the grand
      unification: lx64.c is a focused LX-loader port that parses MZ/LX,
      loads objects at their preferred base in the user-granted first
      2MB, applies internal + import fixups, and resolves DOSCALLS
      imports to fixed-slot 32-bit int-0x80 thunks (thunk32.asm/.h).
      The 64-bit kernel runs hello_os2.exe - a genuine OS/2 LX binary,
      the same one the 32-bit kernel runs - in compat mode: DosWrite via
      DOSCALLS.282 printed, DosExit clean. `make run64` demos a native
      ELF64 app AND an OS/2 LX app in one boot. KEY FIX: the user data
      segment (GDT 0x20) needs B=1 (0x00CFF200...) so 32-bit compat
      pushes use ESP not SP - otherwise the stack pointer is wrong ✅
- [x] B.5 PREEMPTIVE MULTITASKING (July 2026): round-robin scheduler
      (sched64.c) with per-task register context + kernel stacks, a
      context-switching timer IRQ stub (irq64_sched saves 15 GPRs,
      schedule64 swaps RSP), and SYS_PUTC/SYS_EXIT for tasks. Three
      ring-3 tasks run concurrently, timer-preempted at 100 Hz, output
      cleanly interleaved (BCACAB...ABCABC). `make run64mt` demos it.
      KEY FIX: TSS.RSP0 must be repointed to the incoming task's own
      kernel stack on every switch - otherwise all tasks' interrupt
      frames land on one shared stack and clobber each other ✅
- [x] B.6 KEYBOARD + INTERACTIVE SHELL (July 2026): PS/2 keyboard IRQ1
      driver (kbd64.c, scancode set 1 -> ASCII, shift/caps, ring buffer,
      blocking getchar), a scrolling framebuffer text console (8x16
      cells, mirrors to serial), and a command shell (help/ver/mem/
      ticks/clear/echo/smp). Driven live by keyboard on the 1024x768
      display; `smp` launches the multitasking demo from the shell.
      `make run64sh` boots to the shell, `make run64mt` auto-runs the
      demo via "-append smp" (cmdline-gated) ✅
- [x] B.7 GRAPHICAL DESKTOP ON THE 64-BIT KERNEL (July 2026): PS/2
      mouse driver (mouse64.c, IRQ12, 3-byte packets, cursor tracking),
      and a native OS/2-style desktop drawn by the 64-bit kernel - teal
      Warp gradient, titled windows with close boxes, taskbar with Start
      button and live uptime clock, arrow cursor with save-under. `gui`
      shell command enters it, Esc returns. The high-res OS/2 look, now
      on the 64-bit foundation - both worlds, one kernel ✅
      KEY FIX: mouse init must run with interrupts masked (cli/sti),
      else the IRQ12 handler steals the command ACK bytes the polling
      loop waits for and the kernel hangs.
- [x] B.8 WINDOW MANAGER (July 2026): background-buffer compositor
      (bg_buffer, full recompose per frame = no drag trails), multiple
      overlapping windows with z-order, click-to-raise focus (brighter
      title bar on top), title-bar dragging, and working [x] close
      boxes. Verified: clicked+raised the "About" window and dragged it
      by its title bar from (260,230) to (500,470), revealing the window
      behind it. Note: QEMU relative-pointer events are only 1:1 in
      small steps (large deltas get acceleration) - relevant for
      scripted QMP testing, not real use ✅
- [x] B.9 OS/2 WORKPLACE SHELL DESKTOP + APP SDK (July 2026):
      - Desktop redesigned to look like OS/2 Warp: teal desktop with
        Workplace-Shell icons down the left (OS/2 System, Drives,
        Programs, Information, Shredder), double-click to open folder
        windows, and authentic OS/2 window chrome (system-menu box,
        min/max boxes, raised 3D bevel frame, active/inactive titles).
      - `sdk/` - a real application SDK: os2.h (OS/2 API + helpers),
        build-app.sh (wcl386 -bt=os2 -l=os2v2 -> LX .exe), embed-app.py
        (embed into RamFS), three worked examples (hello/fileio/clock),
        and a tutorial README. Verified: an SDK-built hello.exe runs on
        the kernel and prints its output. ✅
- [ ] Future: real app windows (shell/terminal in a window) on the
      64-bit desktop; Start menu; disk-backed FS so apps load without
      recompiling the kernel; port the full 32-bit GUI; boot polish
- [ ] B.2 Memory: 4-level page tables (PML4), higher-half kernel at
      0xFFFFFFFF80000000, rewrite pmm/vmm for 64-bit physical addresses
- [ ] B.3 CPU structures: 64-bit GDT/IDT/TSS (IST stacks), rewrite
      isr.asm/switch.asm interrupt frames for 64-bit
- [ ] B.4 Syscalls: SYSCALL/SYSRET fast path alongside int 0x80 compat
- [ ] B.5 ELF64 loader; keep ELF32 + LX loaders running in compatibility
      mode (CS.L=0 segments for 32-bit user code)
- [ ] B.6 Drivers: audit all uint32_t pointer assumptions (framebuffer,
      virtio, PCI BARs above 4GB)

Recommended order: finish A.2 items on the 32-bit kernel first (they are
architecture-independent logic), then do Phase B.

---

**Welcome to OS/Two - Where Classic Meets Modern!**

*The open-source OS/2-compatible operating system for the 21st century.*
