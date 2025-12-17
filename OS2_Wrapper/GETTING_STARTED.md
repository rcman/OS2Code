# Getting Started with OS/2 Wrapper

## Prerequisites

You need a C compiler (gcc) and make installed:
```bash
# Fedora/RHEL
sudo dnf install gcc make

# Ubuntu/Debian
sudo apt install build-essential

# Arch
sudo pacman -S base-devel
```

## Building

Simply run:
```bash
make
```

This builds three programs:
- **os2run** - Full OS/2 loader with API emulation (use this for running OS/2 apps)
- **os2loader** - Basic loader that parses and loads binaries (for debugging)
- **os2_emulator** - Standalone API test program

To clean build artifacts:
```bash
make clean
```

## Running the API Test

To verify the API emulation layer works:
```bash
make test
```

Or directly:
```bash
./os2_emulator
```

This tests file I/O and memory allocation APIs.

## Running an OS/2 Executable

```bash
./os2run /path/to/os2_program.exe
```

## Finding OS/2 Test Executables

### Option 1: Simple Test Programs

You can create simple OS/2 executables using the Open Watcom compiler:
- Download from: https://github.com/open-watcom/open-watcom-v2/releases
- Cross-compile OS/2 binaries from Linux

Example simple program:
```c
#define INCL_DOS
#include <os2.h>

int main() {
    HFILE hf;
    ULONG action;
    ULONG bytes;
    char buffer[256] = "Hello from OS/2!\n";

    DosOpen("test.txt", &hf, &action, 0,
            FILE_NORMAL,
            OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
            OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYNONE,
            NULL);

    DosWrite(hf, buffer, strlen(buffer), &bytes);
    DosClose(hf);
    DosExit(EXIT_PROCESS, 0);
    return 0;
}
```

### Option 2: Existing OS/2 Software

Public domain OS/2 binaries can be found at:
- **Hobbes OS/2 Archive**: https://hobbes.nmsu.edu/
  - Contains thousands of OS/2 applications and utilities
  - Look for command-line utilities in the `/pub/os2/util/` directory
  - Simple utilities are more likely to work than GUI apps

- **eComStation/ArcaOS Demos**: Limited trial versions available
  - Some open source OS/2 tools available for testing

### Option 3: Extract from OS/2 Installation

If you have access to an OS/2 installation or ISO:
- Extract .exe files from the OS/2 installation media
- Start with simple command-line tools from `C:\OS2\`
- Good candidates: `cmd.exe` utilities, `*.dll` files for testing

## Current Limitations

**IMPORTANT**: This wrapper is experimental. Many OS/2 programs will NOT work because:

1. **No fixup/relocation processing** - Most real executables need this
2. **Limited API coverage** - Only 12 APIs implemented (DosOpen, DosRead, DosWrite, DosClose, DosSetFilePtr, DosDelete, DosExit, DosSleep, DosAllocMem, DosFreeMem, etc.)
3. **No GUI support** - Only command-line programs can potentially work
4. **No dynamic linking** - Import table patching is incomplete
5. **No path translation** - Drive letters (C:, D:) not handled
6. **No threading** - Minimal thread support only

**Best chance of success**: Simple command-line tools that only use basic file I/O.

## Debugging

To see what's happening when loading an OS/2 executable:
```bash
./os2run your_program.exe 2>&1 | less
```

The loader prints detailed information about:
- LE header parsing
- Object/segment loading
- Import resolution
- API calls being made

## Next Steps

To make this more functional, you would need to implement:
1. Fixup/relocation processing (see `apply_fixups()` stub in os2_with_api.c)
2. More OS/2 APIs (see CLAUDE.md for how to add new APIs)
3. Import Address Table patching
4. Better error handling

See CLAUDE.md for architectural details and development guidance.
