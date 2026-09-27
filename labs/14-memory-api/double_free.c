// double_free.c —— 重复释放（double free）。
#include <stdio.h>
#include <stdlib.h>
int main(void) {
    int *x = malloc(10 * sizeof(int));
    free(x);
    printf("freed once, freeing again...\n");
    fflush(stdout);
    free(x);                             // 未定义行为：glibc 通常会检测到并 abort
    return 0;
}
