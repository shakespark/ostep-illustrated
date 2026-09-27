// dangling.c —— 作业第 6 题：释放后继续使用（悬挂指针 dangling pointer）。
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    int *data = malloc(100 * sizeof(int));
    for (int i = 0; i < 100; i++) data[i] = i;
    free(data);                          // data 仍然保存着原来的地址
    int *other = malloc(100 * sizeof(int));  // 很可能拿到同一块内存
    other[5] = 12345;
    printf("data=%p other=%p data[5]=%d\n", (void *) data, (void *) other, data[5]);
    free(other);
    return 0;
}
