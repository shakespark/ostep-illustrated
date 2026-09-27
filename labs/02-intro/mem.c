// mem.c —— 原书 Figure 2.3：malloc 一个 int，打印它的（虚拟）地址，然后每秒加 1
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"

int
main(int argc, char *argv[])
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    int *p = malloc(sizeof(int));                       // a1
    assert(p != NULL);
    printf("(%d) address pointed to by p: %p\n",
           getpid(), (void *) p);                       // a2
    *p = 0;                                             // a3
    while (1) {
        Spin(1);
        *p = *p + 1;
        printf("(%d) p: %d\n", getpid(), *p);           // a4
    }
    return 0;
}
