/* os2.h - OS/Two application SDK header
 *
 * A small OS/2-style API for writing applications that run on OS/Two.
 * Programs built with this header and linked as OS/2 LX executables
 * (see build-app.sh) run on both the 32-bit OS/Two kernel and, in
 * compatibility mode, the 64-bit kernel.
 *
 * The functions are thin wrappers over the DOSCALLS imports the kernel
 * resolves to int-0x80 syscalls. Link against DOSCALLS by ordinal with
 * the wlink directives shown in the examples' .lnk files.
 *
 * Calling convention: __syscall (a.k.a. _System) - the standard OS/2
 * ABI. APIRET return codes are 0 on success (NO_ERROR).
 */

#ifndef OSTWO_OS2_H
#define OSTWO_OS2_H

typedef unsigned long  ULONG;
typedef unsigned short USHORT;
typedef unsigned char  UCHAR;
typedef unsigned long  APIRET;
typedef unsigned long  HFILE;
typedef void          *PVOID;
typedef char          *PSZ;
typedef ULONG         *PULONG;

/* Standard handles */
#define HFILE_STDIN   0
#define HFILE_STDOUT  1
#define HFILE_STDERR  2

/* DosOpen open flags (open_flags) */
#define OPEN_ACTION_FAIL_IF_EXISTS     0x0000
#define OPEN_ACTION_OPEN_IF_EXISTS     0x0001
#define OPEN_ACTION_CREATE_IF_NEW      0x0010

/* DosOpen open modes (open_mode, access bits) */
#define OPEN_ACCESS_READONLY           0x0000
#define OPEN_ACCESS_WRITEONLY          0x0001
#define OPEN_ACCESS_READWRITE          0x0002

/* DosSetFilePtr origins */
#define FILE_BEGIN     0
#define FILE_CURRENT   1
#define FILE_END       2

/* DosExit actions */
#define EXIT_THREAD    0
#define EXIT_PROCESS   1

/* PAG_ flags for DosAllocMem */
#define PAG_READ       0x0001
#define PAG_WRITE      0x0002
#define PAG_COMMIT     0x0010

/* OS/2 DATETIME (matches the kernel's layout) */
typedef struct {
    UCHAR  hours, minutes, seconds, hundredths;
    UCHAR  day, month;
    USHORT year;
    short  timezone;
    UCHAR  weekday;
} DATETIME, *PDATETIME;

/* ---- Control Program (DOSCALLS) API ---- */
APIRET __syscall DosWrite(HFILE handle, const void *buf, ULONG len, PULONG written);
APIRET __syscall DosRead(HFILE handle, void *buf, ULONG len, PULONG read);
void   __syscall DosExit(ULONG action, ULONG result);
APIRET __syscall DosSleep(ULONG msec);
APIRET __syscall DosBeep(ULONG freq, ULONG dur);
APIRET __syscall DosOpen(PSZ name, HFILE *ph, PULONG action, ULONG cbFile,
                         ULONG attr, ULONG openflags, ULONG openmode, PVOID ea);
APIRET __syscall DosClose(HFILE handle);
APIRET __syscall DosSetFilePtr(HFILE handle, long dist, ULONG origin, PULONG actual);
APIRET __syscall DosAllocMem(PVOID *base, ULONG size, ULONG flags);
APIRET __syscall DosFreeMem(PVOID base);
APIRET __syscall DosGetDateTime(PDATETIME pdt);

/* ---- Convenience helpers (header-only) ---- */

/* Print a NUL-terminated string to stdout. */
static void os2_print(const char *s)
{
    ULONG n = 0, written;
    while (s[n]) n++;
    DosWrite(HFILE_STDOUT, s, n, &written);
}

/* Print an unsigned integer in decimal. */
static void os2_print_uint(ULONG v)
{
    char b[12];
    int i = 0;
    if (v == 0) { os2_print("0"); return; }
    while (v) { b[i++] = (char)('0' + v % 10); v /= 10; }
    { char out[13]; int j = 0; while (i) out[j++] = b[--i]; out[j] = 0; os2_print(out); }
}

#endif /* OSTWO_OS2_H */
