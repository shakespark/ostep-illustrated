// mem.c —— 分配 N MB 内存并反复逐页访问，报告每一轮的耗时、带宽和缺页次数
// 仿 OSTEP 第 21 章 Homework 的 mem.c，额外用 getrusage 统计 minor/major page fault。
// 用法：./mem <MB> [轮数，默认 5]      配合另一个终端里的 `vmstat 1` 观察 swpd/free/si/so
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>

static double now(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec + t.tv_usec / 1e6; }

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: %s <MB> [loops]\n", argv[0]); return 1; }
    long mb = atol(argv[1]);
    int loops = argc > 2 ? atoi(argv[2]) : 5;
    size_t n = (size_t)mb * 1024 * 1024 / sizeof(int);
    int *a = malloc(n * sizeof(int));          // 只分配虚拟地址空间，物理页此时还没有
    if (!a) { perror("malloc"); return 1; }
    printf("allocated %ld MB (%zu pages of 4KB) at %p\n", mb, n * sizeof(int) / 4096, (void *)a);
    struct rusage r0, r1;
    for (int l = 0; l < loops; l++) {
        getrusage(RUSAGE_SELF, &r0);
        double t0 = now();
        for (size_t i = 0; i < n; i++) a[i] += 1;  // 触碰每一个 int（顺序访问）
        double t1 = now();
        getrusage(RUSAGE_SELF, &r1);
        printf("loop %d: %8.2f ms  %9.1f MB/s  minor faults %7ld  major faults %ld\n",
               l, (t1 - t0) * 1000, mb / (t1 - t0),
               r1.ru_minflt - r0.ru_minflt, r1.ru_majflt - r0.ru_majflt);
    }
    free(a);
    return 0;
}
