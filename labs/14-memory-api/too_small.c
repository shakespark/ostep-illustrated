// too_small.c —— 分配不足（缓冲区溢出）：忘了给 '\0' 留 1 个字节。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    char *src = "hello";
    char *dst = (char *) malloc(strlen(src));   // too small! 需要 strlen+1
    strcpy(dst, src);                           // 多写了 1 个字节
    printf("dst = %s\n", dst);                  // 不带工具运行时往往"看起来没问题"
    free(dst);
    return 0;
}
