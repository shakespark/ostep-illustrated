// segtrans.c —— 原书 16.2/16.3 节的分段地址转换，用 C 逐步实现。
// 14 位虚拟地址：高 2 位 = 段号，低 12 位 = 段内偏移；每段最大 4KB。
// 段表取自图 16.4：Code(00) 32K/2K 正向、Heap(01) 34K/3K 正向、Stack(11) 28K/2K 反向增长。
#include <stdio.h>
#include <stdlib.h>

#define SEG_MASK     0x3000
#define SEG_SHIFT    12
#define OFFSET_MASK  0xFFF
#define MAX_SEG_SIZE 4096

struct seg { const char *name; long base, size; int grows_positive, valid; };
struct seg Seg[4] = {
    {"Code",  32 * 1024, 2 * 1024, 1, 1},
    {"Heap",  34 * 1024, 3 * 1024, 1, 1},
    {"(unused)", 0, 0, 1, 0},
    {"Stack", 28 * 1024, 2 * 1024, 0, 1},
};

void translate(long va) {
    int segment = (va & SEG_MASK) >> SEG_SHIFT;        // 取高 2 位
    long offset = va & OFFSET_MASK;                     // 取低 12 位
    struct seg *s = &Seg[segment];
    printf("VA %5ld = 0x%04lx  seg=%d%d(%-8s) offset=%4ld", va, va, segment >> 1, segment & 1, s->name, offset);
    if (!s->valid) { printf("  --> FAULT: segment not in use\n"); return; }
    if (s->grows_positive) {
        if (offset >= s->size) { printf("  --> FAULT: offset %ld >= size %ld\n", offset, s->size); return; }
        printf("  --> PA %ld (base %ld + %ld)\n", s->base + offset, s->base, offset);
    } else {
        long neg = offset - MAX_SEG_SIZE;               // 反向增长：负偏移
        if (-neg > s->size) { printf("  neg=%ld --> FAULT: |%ld| > size %ld\n", neg, neg, s->size); return; }
        printf("  neg=%ld --> PA %ld (base %ld %ld)\n", neg, s->base + neg, s->base, neg);
    }
}

int main(int argc, char *argv[]) {
    if (argc > 1) { for (int i = 1; i < argc; i++) translate(strtol(argv[i], NULL, 0)); return 0; }
    long tests[] = {100, 4200, 7 * 1024, 15 * 1024, 14 * 1024, 14 * 1024 - 1, 0x2000, 2047, 2048};
    for (unsigned i = 0; i < sizeof tests / sizeof tests[0]; i++) translate(tests[i]);
    return 0;
}
