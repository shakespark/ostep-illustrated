// overflow.c —— 作业第 5 题：data 有 100 个元素，却写 data[100]。
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    int *data = malloc(100 * sizeof(int));
    data[100] = 0;               // 越界 1 个元素（合法下标是 0..99）
    printf("wrote data[100], program still running\n");
    free(data);
    return 0;
}
