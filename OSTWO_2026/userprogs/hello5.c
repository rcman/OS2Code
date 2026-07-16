/* hello5.c - C file I/O and argv test for OSTwo.
 * Standard C: argc/argv from the OS/2 command line, fopen/fprintf
 * writing a file through DosOpen/DosWrite, then reading it back. */
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *f;
    char buf[128];
    int i;

    printf("argc = %d\n", argc);
    for (i = 0; i < argc; i++) {
        printf("argv[%d] = \"%s\"\n", i, argv[i]);
    }

    f = fopen("test.txt", "w");
    if (f == NULL) {
        printf("fopen for write FAILED\n");
        return 1;
    }
    fprintf(f, "Written through fopen/fprintf on OSTwo.\n");
    fprintf(f, "argc was %d.\n", argc);
    fclose(f);
    printf("Wrote test.txt via the OS/2 file API.\n");

    f = fopen("test.txt", "r");
    if (f == NULL) {
        printf("fopen for read FAILED\n");
        return 1;
    }
    while (fgets(buf, sizeof(buf), f) != NULL) {
        printf("read back: %s", buf);
    }
    fclose(f);

    printf("File I/O through DosOpen/DosRead/DosWrite works!\n");
    return 0;
}
