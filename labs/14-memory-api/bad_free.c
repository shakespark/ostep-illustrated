// bad_free.c —— 作业第 7 题：把不是 malloc 返回的指针交给 free（invalid free）。
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    int *data = malloc(100 * sizeof(int));
    printf("freeing a pointer into the middle of the array...\n");
    fflush(stdout);
    free(data + 50);                     // 不是块的起始地址，头块（header）找错了
    return 0;
}
