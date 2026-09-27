// ptcost.c —— 用 /proc/self/status 里的 VmPTE（本进程页表占用的内存）观察多级页表的空间开销
// 编译运行：make && ./ptcost
// x86-64 用 4 级页表：每个页表页 4KB，装 512 个 8 字节的项；一个最底层页表页管 512 × 4KB = 2MB 的虚拟地址。
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

static long vm_pte_kb(void) {                 // 读取 VmPTE（单位 kB）
    FILE *f = fopen("/proc/self/status", "r");
    char line[256];
    long kb = -1;
    while (f && fgets(line, sizeof line, f))
        if (sscanf(line, "VmPTE: %ld kB", &kb) == 1) break;
    if (f) fclose(f);
    return kb;
}

// 在一块 size 字节的虚拟内存里，每隔 stride 字节写 1 个字节，共写 n 次；返回页表增加了多少 kB
static long touch(size_t size, size_t stride, long n, const char *what) {
    char *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); exit(1); }
    madvise(p, size, MADV_NOHUGEPAGE);        // 只用 4KB 小页，否则内核可能用 2MB 大页、省掉最底层页表
    long before = vm_pte_kb();
    for (long i = 0; i < n; i++) p[i * stride] = 1;
    long after = vm_pte_kb();
    // 中文宽度不一，说明文字放在最后，数字列才能对齐
    printf("数据 %5ld KB   页表 +%5ld KB = %4ld 个页表页   %s\n", n * 4, after - before, (after - before) / 4, what);
    return after - before;                     // 故意不 munmap：让后面的测量互不干扰
}

int main(void) {
    long page = sysconf(_SC_PAGESIZE);
    printf("页大小 %ld 字节；开始时本进程页表占用 VmPTE = %ld kB\n\n", page, vm_pte_kb());

    // 实验 1：连续的 512 页（2MB）——正好落在 1 个最底层页表页里
    long a = touch(1UL << 30, page, 512, "1) 连续 512 页（2MB）");
    // 实验 2：同样 512 页，但每页相隔 2MB，分散在 1GB 里——每页都要一个自己的最底层页表页
    long b = touch(1UL << 30, 2UL << 20, 512, "2) 512 页，每页相隔 2MB（跨 1GB）");
    // 实验 3：512 页，每页相隔 1GB，分散在 512GB 里——连第二级页表页也要各自一个
    long c = touch(512UL << 30, 1UL << 30, 512, "3) 512 页，每页相隔 1GB（跨 512GB）");

    printf("\n同样是 2MB 数据：页表开销 %ld KB → %ld KB → %ld KB。\n", a, b, c);
    printf("多级页表只为\"用到的区域\"分配页表页：越稀疏，每个有效页分摊的页表越多。\n");
    printf("对比：48 位地址空间的线性页表需要 2^36 项 × 8 字节 = 512 GB，而 VmPTE 只有几 MB。\n");
    return 0;
}
