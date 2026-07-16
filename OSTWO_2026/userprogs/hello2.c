/* hello2.c - genuine OS/2 program, built with Open Watcom wcc386/wlink.
 *
 * Uses the real OS/2 system calling convention (__syscall = _System)
 * and imports DOSCALLS entry points by ordinal, like any OS/2 Warp
 * application. No C runtime - the entry point calls the API directly.
 *
 * Build (see build2.sh):
 *   wcc386 -bt=os2 -s -zl -oxs hello2.c
 *   wlink @hello2.lnk
 */

typedef unsigned long ULONG;
typedef ULONG APIRET;

APIRET __syscall DosBeep(ULONG freq, ULONG dur);
APIRET __syscall DosWrite(ULONG handle, const void *buf, ULONG len, ULONG *written);
void   __syscall DosExit(ULONG action, ULONG result);
APIRET __syscall DosSleep(ULONG msec);

static const char msg[] =
    "Greetings from a genuine Open Watcom OS/2 executable!\r\n"
    "Compiled by wcc386, linked by wlink as FORMAT OS2 LX.\r\n";

void __syscall os2main(void)
{
    ULONG written;

    DosBeep(880, 120);
    DosWrite(1, msg, sizeof(msg) - 1, &written);
    DosSleep(50);
    DosExit(1, 0);   /* EXIT_PROCESS, rc 0 */
}
