/* hello.c - the classic first OS/Two application.
 *
 * Uses the OS/Two SDK (os2.h): prints a greeting, beeps, and exits.
 * Build:  ./build-app.sh examples/hello.c
 * Run:    embed hello.exe into the kernel's RamFS (see sdk/README.md)
 *         then, in the OS/Two shell:  exec hello.exe
 */

#include <os2.h>

int main(void)
{
    os2_print("Hello from an OS/Two application!\r\n");
    os2_print("Built with the OS/Two SDK and the OS/2 DOSCALLS API.\r\n");

    DosBeep(880, 120);

    return 0;
}
