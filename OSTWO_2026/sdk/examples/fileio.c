/* fileio.c - reading and writing files with the OS/Two SDK.
 *
 * Creates a file, writes to it, then reads it back and prints it -
 * exercising DosOpen / DosWrite / DosSetFilePtr / DosRead / DosClose.
 */

#include <os2.h>

int main(void)
{
    HFILE  f;
    ULONG  action, written, nread;
    char   buf[128];
    const char *text = "This line was written by fileio.exe.\r\n";

    os2_print("fileio: creating NOTES.TXT ...\r\n");

    if (DosOpen("NOTES.TXT", &f, &action, 0, 0,
                OPEN_ACTION_CREATE_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                OPEN_ACCESS_READWRITE, 0) != 0) {
        os2_print("  DosOpen failed.\r\n");
        return 1;
    }

    { ULONG n = 0; while (text[n]) n++; DosWrite(f, text, n, &written); }
    os2_print("  wrote ");
    os2_print_uint(written);
    os2_print(" bytes.\r\n");

    /* rewind and read it back */
    DosSetFilePtr(f, 0, FILE_BEGIN, &action);
    if (DosRead(f, buf, sizeof(buf) - 1, &nread) == 0 && nread > 0) {
        buf[nread] = 0;
        os2_print("  read back: ");
        os2_print(buf);
    }

    DosClose(f);
    os2_print("fileio: done.\r\n");
    return 0;
}
