/* hello3.c - OS/2 memory & clock API test, built with Open Watcom.
 *
 * Exercises DosAllocMem/DosFreeMem (DOSCALLS.299/.304) and
 * DosGetDateTime (DOSCALLS.230) on OSTwo. No C runtime.
 */

typedef unsigned long ULONG;
typedef unsigned short USHORT;
typedef unsigned char UCHAR;
typedef ULONG APIRET;

typedef struct {
    UCHAR  hours, minutes, seconds, hundredths;
    UCHAR  day, month;
    USHORT year;
    short  timezone;
    UCHAR  weekday;
} DATETIME;

#define PAG_READ    0x0001
#define PAG_WRITE   0x0002
#define PAG_COMMIT  0x0010

APIRET __syscall DosWrite(ULONG handle, const void *buf, ULONG len, ULONG *written);
void   __syscall DosExit(ULONG action, ULONG result);
APIRET __syscall DosAllocMem(void **base, ULONG size, ULONG flags);
APIRET __syscall DosFreeMem(void *base);
APIRET __syscall DosGetDateTime(DATETIME *pdt);

static void print(const char *s)
{
    ULONG written, len = 0;
    while (s[len]) len++;
    DosWrite(1, s, len, &written);
}

void __syscall os2main(void)
{
    void *mem = 0;
    DATETIME dt;

    /* --- DosAllocMem / DosFreeMem --- */
    if (DosAllocMem(&mem, 8192, PAG_READ | PAG_WRITE | PAG_COMMIT) == 0 && mem) {
        char *p = (char *)mem;
        const char *src = "DosAllocMem: writing this line from freshly allocated memory!\r\n";
        int i = 0;
        while (src[i]) { p[i] = src[i]; i++; }
        p[i] = 0;
        print(p);
        if (DosFreeMem(mem) == 0) {
            print("DosFreeMem: released the allocation cleanly.\r\n");
        }
    } else {
        print("DosAllocMem FAILED\r\n");
    }

    /* --- DosGetDateTime --- */
    if (DosGetDateTime(&dt) == 0) {
        char line[48];
        int n = 0;
        const char *pfx = "DosGetDateTime: ";
        while (*pfx) line[n++] = *pfx++;
        line[n++] = '0' + (dt.year / 1000) % 10;
        line[n++] = '0' + (dt.year / 100) % 10;
        line[n++] = '0' + (dt.year / 10) % 10;
        line[n++] = '0' + dt.year % 10;
        line[n++] = '-';
        line[n++] = '0' + dt.month / 10;
        line[n++] = '0' + dt.month % 10;
        line[n++] = '-';
        line[n++] = '0' + dt.day / 10;
        line[n++] = '0' + dt.day % 10;
        line[n++] = ' ';
        line[n++] = '0' + dt.hours / 10;
        line[n++] = '0' + dt.hours % 10;
        line[n++] = ':';
        line[n++] = '0' + dt.minutes / 10;
        line[n++] = '0' + dt.minutes % 10;
        line[n++] = '\r';
        line[n++] = '\n';
        line[n] = 0;
        print(line);
    } else {
        print("DosGetDateTime FAILED\r\n");
    }

    DosExit(1, 0);
}
