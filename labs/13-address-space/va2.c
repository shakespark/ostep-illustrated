// va2.c —— 把地址空间里的几块区域按地址从低到高排出来。
// 注意：小的 malloc 来自 brk 堆（紧跟在数据段后面）；
// 很大的 malloc（如 va.c 里的 100e6 字节）glibc 会改用 mmap，落在高处靠近共享库的地方。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int global_init = 42;      // 已初始化数据（.data）
int global_zero;           // 未初始化数据（.bss）

int main(void) {
    int local = 3;
    void *small = malloc(100);          // 小块：brk 堆
    void *big   = malloc(100 * 1000 * 1000); // 大块：mmap 区域
    printf("pid %d\n", getpid());
    printf("code  (main)        : %p\n", (void *) main);
    printf("data  (global_init) : %p\n", (void *) &global_init);
    printf("bss   (global_zero) : %p\n", (void *) &global_zero);
    printf("heap  (malloc 100)  : %p\n", small);
    printf("mmap  (malloc 100MB): %p\n", big);
    printf("stack (local)       : %p\n", (void *) &local);
    free(small); free(big);
    return 0;
}
