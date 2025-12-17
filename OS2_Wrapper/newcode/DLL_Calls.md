# How 2ine Resolves DLL Dependencies

## The Problem

Your OS/2 app wants to call:
```
PMWIN.DLL → WinCreateWindow()
DOSCALLS.DLL → DosOpen()
VIOCALLS.DLL → VioWrtTTY()
```

But you're on Linux - these DLLs don't exist!

## How 2ine Solves This

2ine **doesn't actually load real DLL files**. Instead, it:

1. **Intercepts** the DLL import requests
2. **Maps** OS/2 API calls to Linux equivalents
3. **Provides stub implementations** for each function

Think of it like Wine for Windows - it reimplements the Windows API on Linux. 2ine does the same for OS/2.

---

## The 2ine Architecture

```
┌─────────────────────────────────────┐
│   Your OS/2 Application (myapp.exe) │
│                                      │
│   Calls: DosOpen("file.txt")        │
└──────────────┬───────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│         lx_loader (2ine core)        │
│                                      │
│   • Loads LX executable into memory  │
│   • Parses import table              │
│   • Routes function calls            │
└──────────────┬───────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│      lib2ine.c (API Bridge)          │
│                                      │
│   Maps OS/2 → Linux:                 │
│   • LX_DosOpen() → open()            │
│   • LX_VioWrtTTY() → write()         │
│   • LX_WinCreateWindow() → ???       │
└──────────────┬───────────────────────┘
               │
               ▼
┌─────────────────────────────────────┐
│          Linux Kernel                │
│                                      │
│   Native system calls                │
└─────────────────────────────────────┘
```

---

## Step-by-Step: Implementing a DLL Function

### Step 1: Find What Functions Are Needed

```bash
# Enhanced analyzer to show specific functions
./lx_dump myapp.exe | grep -A 50 "Import Procedure"
```

This shows:
```
Import Module: DOSCALLS
  Ordinal 123: DosOpen
  Ordinal 124: DosRead
  Ordinal 125: DosWrite
  Ordinal 126: DosClose
```

### Step 2: Check If It's Already Implemented

Look in `lib2ine.c`:

```c
// Search for the function
grep -n "DosOpen" lib2ine.c
```

If found:
```c
uint16_t LX_DosOpen(uint8_t *name, uint16_t *handle, ...) {
    // Implementation already exists!
    int fd = open((char*)name, O_RDWR);
    *handle = fd;
    return 0;
}
```

### Step 3: If Missing, Add the Implementation

Edit `lib2ine.c` and add:

```c
// OS/2 API signature (from IBM docs)
uint16_t LX_DosBeep(uint32_t freq, uint32_t duration) {
    // Linux equivalent
    // Option 1: Do nothing (silent stub)
    return 0;
    
    // Option 2: Approximate with system beep
    // printf("\a");
    // usleep(duration * 1000);
    // return 0;
    
    // Option 3: Use PC speaker (requires root)
    // ioctl(console_fd, KIOCSOUND, freq);
    // usleep(duration * 1000);
    // ioctl(console_fd, KIOCSOUND, 0);
    // return 0;
}
```

### Step 4: Register the Function

In `lib2ine.c`, add to the function table:

```c
// DLL function registry
static const struct {
    const char *dll_name;
    const char *func_name;
    uint16_t ordinal;
    void *implementation;
} api_table[] = {
    {"DOSCALLS", "DosOpen", 123, LX_DosOpen},
    {"DOSCALLS", "DosBeep", 286, LX_DosBeep},  // ← Add this
    // ... more functions
};
```

### Step 5: Rebuild and Test

```bash
cd 2ine/build
make
./lx_loader myapp.exe
```

---

## Real Example: Adding VioWrtTTY

Let's implement a commonly needed function:

### 1. Find the API Documentation

From IBM OS/2 Toolkit:
```c
USHORT VioWrtTTY(
    PCH    pch,        // Pointer to string
    USHORT cb,         // Length of string
    HVIO   hvio        // Video handle (always 0)
);
```

### 2. Write Linux Equivalent

```c
uint16_t LX_VioWrtTTY(uint8_t *pch, uint16_t cb, uint16_t hvio) {
    // Simply write to stdout
    if (pch && cb > 0) {
        write(STDOUT_FILENO, pch, cb);
    }
    return 0;  // Success
}
```

### 3. Add to Function Table

```c
{"VIOCALLS", "VioWrtTTY", 31, LX_VioWrtTTY},
```

### 4. Handle Calling Convention

OS/2 uses different calling conventions. 2ine handles this automatically, but you need to be aware:

```c
// OS/2 uses _System calling convention (similar to stdcall)
// Parameters pushed right-to-left
// Callee cleans up stack
// Return value in AX (16-bit) or EAX (32-bit)
```

---

## The Automation Scripts

2ine includes helper scripts:

### `lxapigen.pl` - Generate API stubs

```bash
# Creates skeleton implementations from API definitions
./lxapigen.pl os2api.txt > generated_apis.c
```

### `os2imports.pl` - Analyze what's missing

```bash
# Shows which APIs your app needs but 2ine doesn't have
./os2imports.pl myapp.exe
```

Output:
```
Missing APIs in myapp.exe:
  PMWIN.DLL:
    - WinInitialize (ordinal 716)
    - WinCreateWindow (ordinal 1041)
    - WinMessageBox (ordinal 1010)
  
  PMGPI.DLL:
    - GpiCreatePS (ordinal 110)
    - GpiSetColor (ordinal 215)
```

---

## The Hard Truth About GUI Apps

For **Presentation Manager** (GUI) apps, you'd need to implement **hundreds** of functions:

```c
// Just a tiny sample of PM APIs needed:
LX_WinInitialize()
LX_WinCreateMsgQueue()
LX_WinRegisterClass()
LX_WinCreateWindow()
LX_WinShowWindow()
LX_WinGetMsg()
LX_WinDispatchMsg()
LX_WinPostMsg()
LX_WinSendMsg()
LX_WinBeginPaint()
LX_WinEndPaint()
LX_WinDrawText()
LX_WinDefWindowProc()
// ... 500+ more functions
```

And each function would need to:
- Map to X11/Wayland/SDL
- Handle coordinate system differences
- Manage window hierarchies
- Implement event loops
- Handle GDI drawing operations

This is why 2ine only supports console apps.

---

## Strategy for Your App

### If Console App (VIO-based):

**1. Find missing APIs:**
```bash
./lx_loader myapp.exe 2>&1 | grep "Unimplemented"
```

**2. Implement them one by one:**
```c
// Start simple
uint16_t LX_VioScrollUp(...) {
    // Stub: do nothing
    return 0;
}
```

**3. Iterate until it runs:**
```bash
make && ./lx_loader myapp.exe
```

### If GUI App (PM-based):

**Don't bother implementing PM from scratch.**

Instead:

1. **Use full OS/2 emulation** (VirtualBox + ArcaOS)
2. **Port the app to Linux** (if you have source)
3. **Wait for someone to implement PM in 2ine** (unlikely)

---

## Quick Reference: Common DLL Mappings

| OS/2 DLL | Purpose | Linux Equivalent |
|----------|---------|------------------|
| DOSCALLS | File/process/memory | `open()`, `fork()`, `malloc()` |
| VIOCALLS | Console text I/O | `write()`, terminal escapes |
| KBDCALLS | Keyboard input | `read()`, `termios` |
| MOUCALLS | Mouse input | `gpm`, X11 events |
| PMWIN | GUI windows | X11/Wayland (complex!) |
| PMGPI | Graphics drawing | Cairo/SDL (complex!) |
| REXX | Scripting | Implement interpreter |
| TCP32DLL | Networking | `socket()`, standard BSD |

---

## Resources

**OS/2 API Documentation:**
- IBM OS/2 Programmer's Toolkit
- EDM/2 (http://www.edm2.com/)
- OS/2 Museum technical library

**2ine Source Files:**
- `lib2ine.c` - Main API implementations
- `lx_loader.c` - Executable loader
- `lxapigen.pl` - Code generation tool
- `native/` - Native OS/2 API definitions

**Testing Strategy:**
1. Start with simple console apps
2. Implement APIs as needed
3. Test incrementally
4. Don't attempt PM apps unless you have months to spare
