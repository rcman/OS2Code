Yes, you absolutely can create an interface between Linux and an OS/2 wrapper. Here’s the practical approach:
Core Architecture
You’d build a loader + API translation layer:

OS/2 .exe → Your Loader → Translated API calls → Linux syscalls

Implementation Steps
1. Binary Loader
Parse and load OS/2 executables (LE/NE format):
	•	Read the executable headers
	•	Map segments into memory
	•	Resolve imports/exports
	•	Set up the initial execution state
You can write this in modern C/C++ or Rust. The binary format specifications are available.
2. API Shim Layer
Create shared libraries that mimic OS/2 DLLs but call Linux functions internally:

// Your implementation of DOSCALLS.DLL
APIRET DosOpen(PCSZ pszFileName, PHFILE pHandle, 
               PULONG pulAction, ULONG ulFileSize, ...) {
    // Translate OS/2 parameters to POSIX
    int fd = open(pszFileName, translate_flags(...));
    *pHandle = fd;
    return (fd >= 0) ? NO_ERROR : ERROR_FILE_NOT_FOUND;
}

3. The Interface Bridge
This is where you intercept calls. Two main approaches:
Option A: LD_PRELOAD style interception
	•	Your wrapper loads first
	•	Intercepts function calls meant for OS/2 DLLs
	•	Translates and forwards to Linux
Option B: Direct linking
	•	Compile your own versions of OS/2 runtime libraries
	•	Link OS/2 apps against your Linux-native implementations
	•	Handle the translation at link time
4. Execution Environment
You’ll need to handle:
	•	Thread management (OS/2 threads → pthreads)
	•	Process/memory management differences
	•	File system path translation (drive letters, etc.)
	•	Signal/exception handling
Minimal Viable Example
Start with something simple:

// os2loader.c - minimal loader
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

typedef struct {
    // LE header fields
    uint32_t entry_point;
    // ... other fields
} le_header_t;

void* load_os2_exe(const char* filename) {
    // 1. Read LE header
    // 2. Allocate memory regions
    // 3. Load segments
    // 4. Fix up relocations
    return entry_point;
}

int main(int argc, char** argv) {
    void (*entry)() = load_os2_exe(argv[1]);
    entry(); // Jump to OS/2 code
}

Key Challenge: The Interface
The interface between Linux and your wrapper needs to handle:
	1.	Calling conventions - OS/2 often uses different calling conventions than Linux
	2.	Data structure layouts - Ensure binary compatibility
	3.	System resources - Map OS/2 handles to Linux file descriptors, etc.
You could use function trampolines to bridge between OS/2 and Linux calling conventions.
Practical Suggestion
Start with a proof of concept:
	1.	Write a loader that can parse and load a simple OS/2 executable into memory
	2.	Implement just 5-10 critical API functions (DosOpen, DosRead, DosWrite, DosExit, etc.)
	3.	Test with a minimal OS/2 program
	4.	Gradually expand API coverage
Modern tools like Rust would actually be excellent for this - you get memory safety while still having low-level control for binary parsing and system calls.



