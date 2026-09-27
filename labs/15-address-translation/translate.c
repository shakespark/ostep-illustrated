// translate.c —— 用 C 写出 MMU 做的事：基址 + 界限（base and bounds）地址转换。
// 不带参数：跑原书第 15 章的两组例子；
// 带参数：./translate <base> <bounds> <va> [va ...]   （数字可写 16K、0x3000 等）
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 硬件做的事：先检查界限，再加基址。返回 -1 表示越界异常（fault）
long translate(long va, long base, long bounds) {
    if (va < 0 || va >= bounds) return -1;   // 界限检查：0 <= va < bounds
    return va + base;                        // 重定位：pa = va + base
}

long parse(const char *s) {                  // 支持 "16K" / "16k" / "0x4000" / "3000"
    char *end;
    long v = strtol(s, &end, 0);
    if (*end == 'k' || *end == 'K') v *= 1024;
    if (*end == 'm' || *end == 'M') v *= 1024 * 1024;
    return v;
}

void show(long va, long base, long bounds) {
    long pa = translate(va, base, bounds);
    if (pa < 0) printf("  VA %6ld (0x%05lx) --> SEGMENTATION VIOLATION (va >= bounds %ld)\n", va, va, bounds);
    else        printf("  VA %6ld (0x%05lx) --> PA %6ld (0x%05lx) = %ld + %ld\n", va, va, pa, pa, va, base);
}

int main(int argc, char *argv[]) {
    if (argc >= 4) {
        long base = parse(argv[1]), bounds = parse(argv[2]);
        printf("base = %ld, bounds = %ld\n", base, bounds);
        for (int i = 3; i < argc; i++) show(parse(argv[i]), base, bounds);
        return 0;
    }
    printf("Example 1 (Fig 15.2): 16KB address space at PA 32KB, base=32768 bounds=16384\n");
    long va1[] = {128, 15 * 1024, 132, 135, 15 * 1024};
    const char *what[] = {"fetch movl 0x0(%ebx),%eax", "load  x (15KB)", "fetch addl $0x03,%eax", "fetch movl %eax,0x0(%ebx)", "store x (15KB)"};
    for (int i = 0; i < 5; i++) { printf("%-26s", what[i]); show(va1[i], 32 * 1024, 16 * 1024); }
    printf("\nExample 2: 4KB address space at PA 16KB, base=16384 bounds=4096\n");
    long va2[] = {0, 1024, 3000, 4400};
    for (int i = 0; i < 4; i++) show(va2[i], 16 * 1024, 4 * 1024);
    return 0;
}
