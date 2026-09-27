// present.c —— 在用户态"看见"页表项里的 present 位
// Linux 的 /proc/self/pagemap 为每个虚拟页提供一个 64 位条目：
//   bit 63 = page present（页在物理内存中）   bit 62 = page swapped（页在交换空间中）
// 实验步骤：
// 1) mmap 16 页匿名内存：一页都没有（按需分配，页表项还是空的）
// 2) 写偶数页：触发 minor page fault，页被分配（demand zero）并 present
// 3) madvise(MADV_PAGEOUT) 请求内核把这些页换出（Linux 5.4+，需要有交换空间）
//    → 页表项 present=0，里面记着交换空间里的位置（swapped=1）
// 4) 再读偶数页：触发页错误，OS 把页换回来，present 重新为 1，数据不变
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>

#define NPAGES 16
#ifndef MADV_PAGEOUT
#define MADV_PAGEOUT 21
#endif

static long pg;
static int pm;

static void show(const char *tag, char *p) {
    int present = 0, swapped = 0;
    printf("%-32s ", tag);
    for (int i = 0; i < NPAGES; i++) {
        uint64_t e = 0;
        off_t off = ((uintptr_t)(p + i * pg) / pg) * sizeof(e);
        if (pread(pm, &e, sizeof(e), off) != sizeof(e)) { perror("pread pagemap"); exit(1); }
        if (e >> 63 & 1) { putchar('P'); present++; }
        else if (e >> 62 & 1) { putchar('S'); swapped++; }
        else putchar('.');
    }
    printf("   present=%2d swapped=%2d\n", present, swapped);
}
static void faults(const char *tag, struct rusage *before) {
    struct rusage now; getrusage(RUSAGE_SELF, &now);
    printf("   -> %s: minor faults %ld, major faults %ld\n", tag,
           now.ru_minflt - before->ru_minflt, now.ru_majflt - before->ru_majflt);
    *before = now;
}

int main(void) {
    pg = sysconf(_SC_PAGESIZE);
    pm = open("/proc/self/pagemap", O_RDONLY);
    if (pm < 0) { perror("open /proc/self/pagemap"); return 1; }
    char *p = mmap(NULL, NPAGES * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); return 1; }
    printf("16 pages at %p   (P = present in RAM, S = swapped out, . = never touched)\n", (void *)p);
    struct rusage ru; getrusage(RUSAGE_SELF, &ru);
    show("after mmap:", p);
    for (int i = 0; i < NPAGES; i += 2) p[i * pg] = 'A' + i;
    show("after writing even pages:", p);
    faults("write even pages", &ru);
    if (madvise(p, NPAGES * pg, MADV_PAGEOUT) != 0) perror("madvise(MADV_PAGEOUT)");
    usleep(200 * 1000);  // 给换出 I/O 一点时间
    show("after MADV_PAGEOUT:", p);
    getrusage(RUSAGE_SELF, &ru);
    int ok = 1;
    for (int i = 0; i < NPAGES; i += 2) if (p[i * pg] != 'A' + i) ok = 0;
    show("after reading even pages:", p);
    faults("re-read even pages", &ru);
    printf("data intact after swap-in? %s\n", ok ? "yes" : "NO");
    munmap(p, NPAGES * pg);
    close(pm);
    return 0;
}
