// layout.c —— 看一个进程地址空间里的代码、静态数据、堆、栈各在哪里（原书 Figure 4.1）
// 并演示"懒加载"：64 MiB 的静态数组在被真正访问之前几乎不占物理内存
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int  initialized = 42;                       // 静态数据（.data）：从可执行文件里加载
static char big[64 << 20];                   // 64 MiB 未初始化静态数据（.bss）：不占磁盘空间

static long rss_kb(void) {                   // 读 /proc/self/status 里的 VmRSS（实际驻留内存）
    FILE *f = fopen("/proc/self/status", "r");
    char line[256]; long kb = -1;
    while (f && fgets(line, sizeof line, f))
        if (strncmp(line, "VmRSS:", 6) == 0) { kb = atol(line + 6); break; }
    if (f) fclose(f);
    return kb;
}

int main(int argc, char *argv[]) {
    int local = 7;                           // 栈
    int *heap = malloc(sizeof(int));         // 堆
    printf("代码   main()        @ %p\n", (void *) main);
    printf("静态   initialized   @ %p\n", (void *) &initialized);
    printf("静态   big[] (.bss)  @ %p\n", (void *) big);
    printf("堆     malloc()      @ %p\n", (void *) heap);
    printf("栈     local         @ %p\n", (void *) &local);
    printf("栈     argv[0]=\"%s\" @ %p（OS 在栈上准备好的 argc/argv）\n", argv[0], (void *) argv[0]);

    printf("\n懒加载（lazy）：\n");
    printf("  访问 big[] 之前 VmRSS = %6ld KiB\n", rss_kb());
    memset(big, 1, sizeof big);              // 真正写一遍，操作系统才会逐页分配物理内存
    printf("  写满 big[] 之后 VmRSS = %6ld KiB\n", rss_kb());
    free(heap);
    return 0;
}
