// leak.c —— 作业第 4 题：忘记释放（内存泄漏）。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(void) {
    for (int i = 0; i < 3; i++) {
        char *buf = malloc(1024);        // 每轮分配 1 KB
        strcpy(buf, "some data");
        // 忘了 free(buf); 下一轮 buf 被覆盖，上一块就再也找不到了
    }
    printf("exiting without free\n");
    return 0;                            // 进程退出时 OS 会回收整个地址空间
}
