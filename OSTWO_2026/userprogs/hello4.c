/* hello4.c - standard C program with the full Watcom C runtime,
 * compiled for OS/2: startup code, printf, the works. */
#include <stdio.h>

int main(void)
{
    printf("Full C runtime on OSTwo!\n");
    printf("printf() via the genuine Watcom clib for OS/2.\n");
    printf("The answer is %d and pi is roughly %.2f\n", 42, 3.14159);
    return 0;
}
