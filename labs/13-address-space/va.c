// va.c —— 原书 13 章 ASIDE 里的程序：打印代码、堆、栈的地址。
// 你打印出来的每一个地址都是虚拟地址（virtual address）。
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    printf("location of code : %p\n", (void *) main);
    printf("location of heap : %p\n", malloc(100e6));
    int x = 3;
    printf("location of stack: %p\n", (void *) &x);
    return x;
}
