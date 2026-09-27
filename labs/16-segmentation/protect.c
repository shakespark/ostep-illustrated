// protect.c —— 现代 Linux 上的"段"与保护位（protection bits）。
// 1) 打印 /proc/self/maps 中本程序的代码、只读数据、数据、堆、栈等区域及其权限 r/w/x；
// 2) 尝试写只读区域（字符串常量、代码），观察 CPU 的保护异常 → SIGSEGV。
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <setjmp.h>

static sigjmp_buf env;
static void handler(int sig) { (void) sig; siglongjmp(env, 1); }

int global = 1;

static void try_write(const char *what, volatile char *p) {
    if (sigsetjmp(env, 1) == 0) {
        *p = 'X';
        printf("write to %-22s %p : OK\n", what, (void *) p);
    } else {
        printf("write to %-22s %p : SIGSEGV (protection fault)\n", what, (void *) p);
    }
}

int main(void) {
    signal(SIGSEGV, handler);
    char *heap = malloc(16);
    char *lit = "hello";                  // 字符串常量放在只读段
    int local = 0;

    printf("main=%p  \"hello\"=%p  &global=%p  heap=%p  &local=%p\n\n",
           (void *) main, (void *) lit, (void *) &global, (void *) heap, (void *) &local);

    FILE *f = fopen("/proc/self/maps", "r");
    char line[512];
    while (fgets(line, sizeof line, f))   // 只看本程序、堆和栈
        if (strstr(line, "protect") || strstr(line, "[heap]") || strstr(line, "[stack]"))
            fputs(line, stdout);
    fclose(f);
    printf("\n");

    try_write("heap (rw-p)", heap);
    try_write("global (rw-p)", (char *) &global);
    try_write("string literal (r--p)", lit);
    try_write("code main (r-xp)", (char *) main);
    free(heap);
    return 0;
}
