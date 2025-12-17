# 2ine Setup and Testing Guide

## Prerequisites

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update
sudo apt-get install build-essential cmake git libsdl2-dev

# Or for Fedora/RHEL
sudo dnf install gcc gcc-c++ cmake git SDL2-devel

# Or for Arch
sudo pacman -S base-devel cmake git sdl2
```

## Building 2ine

```bash
# Clone the repository
git clone https://github.com/icculus/2ine.git
cd 2ine

# Create build directory
mkdir build
cd build

# Configure and build
cmake ..
make

# You should now have these executables:
# - lx_loader (the main loader)
# - lx_dump (diagnostic tool)
```

## Testing Your OS/2 Application

### Step 1: Analyze the executable first

```bash
# Use the analyzer I created
gcc -o lx_analyzer lx_analyzer.c
./lx_analyzer your_app.exe
```

This will tell you:
- Application type (Console/GUI)
- Required DLLs
- 2ine compatibility prediction

### Step 2: Try running with 2ine

```bash
# From the 2ine/build directory
./lx_loader /path/to/your_app.exe

# Add verbose output for debugging
./lx_loader -v /path/to/your_app.exe
```

### Step 3: Use lx_dump for detailed analysis

```bash
# Dump all executable information
./lx_dump /path/to/your_app.exe > app_analysis.txt

# This shows:
# - All segments/objects
# - Import tables (DLLs and functions)
# - Entry points
# - Fixup/relocation data
```

## What to Expect

### ✅ If it's a console app:
```
$ ./lx_loader hello.exe
Hello from OS/2!
$
```

### ❌ If it needs GUI (PM):
```
$ ./lx_loader pmapp.exe
[ERROR] Unimplemented API: WinInitialize (PMWIN.DLL)
Segmentation fault
```

### ⚠️ If it partially works:
```
$ ./lx_loader app.exe
[WARNING] Stub function: DosBeep
Some output here...
[ERROR] Unimplemented API: VioWrtTTY
```

## Understanding 2ine's Limitations

### What 2ine CAN do:
- Load LX format executables
- Basic DOS/system call translation
- Simple console I/O (VIO subsystem, partially)
- File operations
- Memory management
- Some threading support

### What 2ine CANNOT do (yet):
- Presentation Manager (GUI) - **Not implemented at all**
- Graphics (GPI subsystem)
- Advanced VIO features
- Multimedia APIs (MMPM/2)
- Most device drivers
- Complex system APIs

## Debugging Failed Applications

### Check what APIs are being called:

```bash
# Run with strace to see system calls
strace ./lx_loader app.exe 2>&1 | grep -i "unimplemented"

# Look for missing imports
./lx_dump app.exe | grep "Import"
```

### Common failure patterns:

1. **Immediate crash** = Missing critical API
2. **Hangs** = Threading/synchronization issue
3. **Garbled output** = VIO/console translation problem
4. **"Unimplemented" messages** = API not in 2ine yet

## Building Test Applications

If you want to test with your own OS/2 apps:

### Using Open Watcom (on OS/2 or with cross-compiler):

```c
// hello.c - Simple console test
#define INCL_DOS
#include <os2.h>
#include <stdio.h>

int main(void) {
    printf("Hello from OS/2!\n");
    DosBeep(1000, 100);
    return 0;
}
```

Compile:
```bash
wcl386 -l=os2v2 hello.c
```

## Alternative: Full OS/2 Emulation

If 2ine doesn't work for your application:

### Option 1: VirtualBox
```bash
# 1. Download ArcaOS or OS/2 Warp 4.52
# 2. Create VM with:
#    - Type: OS/2
#    - RAM: 512MB minimum
#    - VDI disk
# 3. Install OS/2
# 4. Copy your .exe to VM
```

### Option 2: QEMU
```bash
qemu-system-i386 \
    -hda os2.img \
    -m 512 \
    -vga cirrus \
    -boot c
```

### Option 3: DOSBox-X (for older DOS/OS2 apps)
```bash
dosbox-x
# Configure for OS/2 support in dosbox-x.conf
```

## Next Steps

1. **Run the analyzer** on your executable
2. **Check the output** - GUI or Console?
3. **Try 2ine** if it's console-based
4. **Use full emulation** if it needs GUI

## Useful Resources

- 2ine GitHub: https://github.com/icculus/2ine
- OS/2 Museum: http://www.os2museum.com/
- ArcaOS (modern OS/2): https://www.arcanoae.com/
- OS/2 API Reference: EDM/2 or IBM Toolkit docs

## Getting Help

- Check 2ine issues: https://github.com/icculus/2ine/issues
- OS/2 Forums: https://www.os2world.com/
- Email Ryan Gordon: icculus@icculus.org (2ine author)
