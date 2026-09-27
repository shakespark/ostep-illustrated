// glibc_header.c —— 偷看真实的 glibc malloc：每块前面确实有一个头（header）。
// 64 位 glibc 的块头里，紧挨用户指针之前的 8 字节存着"块大小 | 标志位"。
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int main(void) {
    char *a = malloc(100), *b = malloc(100), *c = malloc(100);
    printf("a=%p b=%p c=%p\n", (void *) a, (void *) b, (void *) c);
    printf("b - a = %ld bytes  (请求 100，实际每块占 %ld)\n", (long) (b - a), (long) (b - a));
    printf("malloc_usable_size(a) = %zu\n", malloc_usable_size(a));
    size_t hdr = ((size_t *) a)[-1];                 // 用户指针前 8 字节：size 字段
    printf("header size field of a = 0x%zx -> chunk size %zu, flags %zu\n", hdr, hdr & ~(size_t) 7, hdr & 7);
    free(b);
    char *d = malloc(100);                           // 刚释放的块很可能立刻被复用
    printf("after free(b): d = malloc(100) = %p  (%s)\n", (void *) d, d == b ? "same as b" : "different");
    free(a); free(c); free(d);
    return 0;
}
