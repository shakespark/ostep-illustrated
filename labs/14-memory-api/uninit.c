// uninit.c —— 忘记初始化：读取 malloc 得到的、从没写过的内存。
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    int *old = malloc(8 * sizeof(int));
    for (int i = 0; i < 8; i++) old[i] = 7;      // 先有人用过这块内存……
    free(old);

    int *a = malloc(8 * sizeof(int));            // 很可能拿回同一块，但没有初始化
    printf("a =");
    for (int i = 0; i < 8; i++) printf(" %d", a[i]);   // 读到"碰巧留在那里"的值
    printf("\n");
    free(a);

    int *b = calloc(8, sizeof(int));             // calloc 会清零，读出来一定是 0
    printf("b =");
    for (int i = 0; i < 8; i++) printf(" %d", b[i]);
    printf("\n");
    free(b);
    return 0;
}
