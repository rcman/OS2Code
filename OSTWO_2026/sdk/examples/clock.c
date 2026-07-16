/* clock.c - read the system clock and allocate memory with the SDK.
 *
 * Demonstrates DosGetDateTime and DosAllocMem / DosFreeMem.
 */

#include <os2.h>

static void print2(UCHAR v)          /* zero-padded 2-digit */
{
    char s[3];
    s[0] = (char)('0' + v / 10);
    s[1] = (char)('0' + v % 10);
    s[2] = 0;
    os2_print(s);
}

int main(void)
{
    DATETIME dt;
    PVOID    block = 0;

    if (DosGetDateTime(&dt) == 0) {
        os2_print("System date/time: ");
        os2_print_uint(dt.year);
        os2_print("-"); print2(dt.month);
        os2_print("-"); print2(dt.day);
        os2_print(" ");  print2(dt.hours);
        os2_print(":"); print2(dt.minutes);
        os2_print(":"); print2(dt.seconds);
        os2_print("\r\n");
    }

    if (DosAllocMem(&block, 4096, PAG_READ | PAG_WRITE | PAG_COMMIT) == 0) {
        char *p = (char *)block;
        p[0] = 'O'; p[1] = 'K'; p[2] = 0;
        os2_print("Allocated 4 KB, wrote to it: ");
        os2_print(p);
        os2_print("\r\n");
        DosFreeMem(block);
        os2_print("Freed the block.\r\n");
    }

    return 0;
}
