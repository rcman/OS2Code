# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This repository implements an OS/2 binary loader and API emulation layer for Linux. It allows OS/2 executables (LE/LX format) to run on Linux by translating OS/2 API calls to POSIX/Linux equivalents.

## Build and Run

Each C file is a standalone program that can be compiled independently:

```bash
# Compile the standalone API emulator (test harness only)
gcc -o os2_emulator os2_emulator.c -lpthread

# Compile the basic loader (parses/loads but doesn't execute)
gcc -o os2loader os2_loader.c

# Compile the integrated loader with full API support
gcc -o os2run os2_with_api.c
```

Run the integrated loader with an OS/2 executable:
```bash
./os2run path/to/os2_app.exe
```

## Architecture

### Three Main Components

1. **os2_emulator.c** (529 lines)
   - Standalone OS/2 API emulation layer
   - Implements 12 core OS/2 APIs (DosOpen, DosRead, DosWrite, DosClose, DosSetFilePtr, DosDelete, DosExit, DosSleep, DosAllocMem, DosFreeMem, DosCreateThread, DosWaitThread)
   - Handle translation: Maps OS/2 HFILE handles to Linux file descriptors
   - Error translation: Converts errno to OS/2 error codes
   - Can be compiled with -DTEST_API for standalone API testing

2. **os2_loader.c** (321 lines)
   - Binary loader for OS/2 LE/LX executables
   - Parses MZ and LE headers
   - Reads object table (segments/sections)
   - Allocates memory with proper permissions (RWX flags)
   - Loads objects into memory using mmap()
   - Does NOT execute or handle fixups/relocations

3. **os2_with_api.c** (649 lines) - **PRIMARY IMPLEMENTATION**
   - Complete integration of loader + API emulation
   - Parses LE/LX executable format (MZ header → LE header → object table)
   - Loads all segments/objects into memory with correct protections
   - Implements full API emulation layer (file I/O, memory, process management)
   - Processes import tables to resolve OS/2 DLL function calls
   - Has stub for fixup/relocation processing (apply_fixups function)
   - Calls the OS/2 entry point after loading

### Binary Format Structures

All files define these key structures:
- `mz_header_t`: DOS MZ header (points to LE header via e_lfanew)
- `le_header_t`: LE/LX header (contains object table info, entry point, import tables)
- `object_table_entry_t`: Object/segment descriptors (code/data sections with RWX flags)

### Memory Layout

The loader uses a global `memory_map` array to track loaded objects:
- Maps OS/2 object numbers to Linux memory addresses
- Each object has: base address, size, flags (readable/writable/executable)
- Entry point calculated as: `memory_map[eip_object - 1].base + eip_offset`

### Handle Management (os2_with_api.c and os2_emulator.c)

Global `handle_table` maps OS/2 HFILE handles → Linux file descriptors:
- MAX_HANDLES = 256
- Functions: init_handle_table(), allocate_handle(), get_linux_fd(), free_handle()
- Thread-safe in os2_emulator.c (uses pthread mutex)

### API Translation Layer

OS/2 APIs → Linux equivalents:
- DosOpen → open() with flag translation
- DosRead/DosWrite → read()/write()
- DosSetFilePtr → lseek()
- DosAllocMem → mmap() with PROT_READ|PROT_WRITE
- DosCreateThread → pthread_create()

Flag translations defined via macros:
- OPEN_ACTION_* → O_CREAT, O_EXCL, O_TRUNC
- OPEN_ACCESS_* → O_RDONLY, O_WRONLY, O_RDWR
- FILE_BEGIN/CURRENT/END → SEEK_SET/CUR/END

## Current Limitations

The integrated loader (os2_with_api.c) has these unimplemented features:

1. **Fixup/Relocation Processing**: The apply_fixups() function is a stub. Real OS/2 apps need fixup records processed to patch addresses in loaded code.

2. **Import Table Patching**: Import resolution identifies which DLL functions are needed but doesn't patch the Import Address Table with function pointers.

3. **Limited API Coverage**: Only 12 APIs implemented. Real apps need dozens more (DosQueryPathInfo, DosCreateDir, DosQuerySysInfo, etc.).

4. **No Thread Local Storage**: Thread support is basic (DosCreateThread wraps pthread_create).

5. **Path Translation**: No drive letter handling or OS/2 → Linux path translation.

## Adding New OS/2 APIs

To add a new OS/2 API function:

1. Add the function signature following the pattern:
   ```c
   APIRET APIENTRY DosYourFunction(params...) {
       // Translate parameters
       // Call Linux equivalent
       // Translate return code
       return NO_ERROR;
   }
   ```

2. Add the function name to the resolve_os2_api() function in os2_with_api.c:
   ```c
   if (strcmp(name, "DOSYOURFUNCTION") == 0) return (void*)DosYourFunction;
   ```

3. Define any new OS/2 error codes, flags, or types at the top of the file.

## File Descriptions in Documentation

- **readme.md**: High-level architecture overview and implementation approach
- **os2Loader.md**: Description of the basic loader capabilities
- **os2_emulator.md**: Description of the API emulation layer
- **os2_with_api.md**: Description of the integrated loader + API system

## Testing

Currently no automated test suite. To test:
1. Compile os2_with_api.c
2. Run with a simple OS/2 executable that uses basic file I/O
3. Check output logs for API calls
4. Monitor for segfaults (indicates missing fixups or APIs)
