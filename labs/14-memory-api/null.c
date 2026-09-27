// null.c —— 作业第 1 题：解引用 NULL 指针。
#include <stdio.h>
int main(void) {
    int *p = NULL;
    printf("about to dereference NULL...\n");
    fflush(stdout);
    printf("%d\n", *p);          // 访问虚拟地址 0：没有映射 → 段错误
    return 0;
}
