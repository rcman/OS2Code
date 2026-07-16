# OS/Two Application SDK

Write your own applications for **OS/Two** in C, using the OS/2 API. Programs
built with this SDK are genuine **OS/2 LX executables** — they run on the
32-bit OS/Two kernel and, in 32-bit compatibility mode, on the 64-bit kernel.

```
sdk/
├── include/os2.h      # OS/2-style API (Dos* functions + helpers)
├── examples/
│   ├── hello.c        # greeting + beep
│   ├── fileio.c       # create / write / read a file
│   └── clock.c        # date/time + memory allocation
├── build-app.sh       # compile a .c into a .exe
├── embed-app.py       # turn a .exe into a header for the kernel RamFS
└── README.md          # this file
```

## 1. Prerequisites

You need **Open Watcom v2**, the modern descendant of the compiler used for
real OS/2 development. Grab the Linux x64 build from
<https://github.com/open-watcom/open-watcom-v2/releases> — the release asset
`open-watcom-2_0-c-linux-x64` is a self-extracting zip:

```bash
unzip open-watcom-2_0-c-linux-x64 -d watcom
export WATCOM_BIN="$PWD/watcom/binl64"
```

## 2. Write an app

A minimal program (`examples/hello.c`):

```c
#include <os2.h>

int main(void)
{
    os2_print("Hello from an OS/Two application!\r\n");
    DosBeep(880, 120);
    return 0;
}
```

`os2.h` gives you the OS/2 control-program API — `DosWrite`, `DosRead`,
`DosOpen`, `DosClose`, `DosSetFilePtr`, `DosAllocMem`, `DosFreeMem`,
`DosGetDateTime`, `DosBeep`, `DosSleep`, `DosExit` — plus two convenience
helpers, `os2_print()` and `os2_print_uint()`. Standard C (`printf`, `fopen`,
…) works too, since apps link against the Watcom OS/2 C runtime.

## 3. Build it

```bash
./build-app.sh examples/hello.c      # -> examples/hello.exe
```

The result is a real OS/2 LX `.exe`, only a couple of KB.

## 4. Run it on OS/Two

Apps live in the kernel's in-memory filesystem (RamFS). Embed your `.exe` as a
C header and register it:

```bash
python3 embed-app.py examples/hello.exe > hello_bin.h
cp hello_bin.h ..            # next to kernel.c
```

In `kernel.c`, alongside the other built-in programs:

```c
#include "hello_bin.h"
/* ... in the RamFS setup ... */
ramfs_write("hello.exe", hello_bin, hello_bin_len);
```

Rebuild and run the kernel (`make && make run`), then in the OS/Two shell:

```
OS/Two> exec hello.exe
Hello from an OS/Two application!
```

> RamFS files are up to 24 KB, which is plenty for C-runtime apps. A future
> disk-backed filesystem will let you drop `.exe` files in without recompiling
> the kernel.

## 5. The examples

| Example      | Shows |
|--------------|-------|
| `hello.c`    | Console output (`DosWrite`) and `DosBeep` |
| `fileio.c`   | `DosOpen` / `DosWrite` / `DosSetFilePtr` / `DosRead` / `DosClose` |
| `clock.c`    | `DosGetDateTime` and `DosAllocMem` / `DosFreeMem` |

Build them all:

```bash
for f in examples/*.c; do ./build-app.sh "$f"; done
```

## How it works

`build-app.sh` invokes `wcl386 -bt=os2 -l=os2v2`, producing an LX executable
that imports its API from `DOSCALLS` by ordinal. When you run it, the OS/Two
kernel's LX loader:

1. parses the MZ/LX headers and loads each object at its linked address,
2. applies internal and import relocations (fixups),
3. resolves each `DOSCALLS` import to a small stub that issues an `int 0x80`
   syscall into the kernel.

So your `DosWrite` call becomes a kernel syscall — the same mechanism real
OS/2 used, reproduced in OS/Two. On the 64-bit kernel the identical binary runs
in 32-bit compatibility mode (CS.L=0), exactly as OS/2 ran 16-bit apps on its
32-bit kernel.

Happy hacking!
