// forget_alloc.c —— 忘记分配内存：dst 是一个未初始化的指针。
#include <stdio.h>
#include <string.h>
int main(void) {
    char *src = "hello";
    char *dst;                   // oops! unallocated（值是栈上的垃圾）
    strcpy(dst, src);            // 往一个随机地址写 → 通常段错误
    printf("%s\n", dst);
    return 0;
}
