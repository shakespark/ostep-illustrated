// timer.c —— 测量之前先了解计时器：连续两次读时钟，最小能看到多小的差值？
#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#define HAVE_RDTSC 1
#endif

int main(void) {
    const int N = 1000000;
    // 1) gettimeofday：单位是微秒
    struct timeval a, b;
    long min_tv = 1L << 30, zero_tv = 0;
    for (int i = 0; i < N; i++) {
        gettimeofday(&a, NULL); gettimeofday(&b, NULL);
        long d = (b.tv_sec - a.tv_sec) * 1000000L + (b.tv_usec - a.tv_usec);
        if (d == 0) zero_tv++; else if (d < min_tv) min_tv = d;
    }
    printf("gettimeofday : 连续两次读数相同的比例 %.1f%%，最小非零差值 %ld us\n", 100.0 * zero_tv / N, min_tv);

    // 2) clock_gettime(CLOCK_MONOTONIC)：单位是纳秒
    struct timespec x, y, res;
    clock_getres(CLOCK_MONOTONIC, &res);
    long min_ns = 1L << 30;
    for (int i = 0; i < N; i++) {
        clock_gettime(CLOCK_MONOTONIC, &x); clock_gettime(CLOCK_MONOTONIC, &y);
        long d = (y.tv_sec - x.tv_sec) * 1000000000L + (y.tv_nsec - x.tv_nsec);
        if (d > 0 && d < min_ns) min_ns = d;
    }
    printf("clock_gettime: 声称分辨率 %ld ns，最小非零差值 %ld ns\n", res.tv_nsec, min_ns);

#ifdef HAVE_RDTSC
    // 3) rdtsc：读 CPU 时间戳计数器（单位是"周期"）
    uint64_t min_c = UINT64_MAX;
    for (int i = 0; i < N; i++) {
        uint64_t c1 = __rdtsc(), c2 = __rdtsc();
        if (c2 > c1 && c2 - c1 < min_c) min_c = c2 - c1;
    }
    printf("rdtsc        : 连续两次最小差值 %llu 个时钟周期\n", (unsigned long long) min_c);
#endif
    printf("\n结论：gettimeofday 只能精确到微秒，而一次系统调用只要几百纳秒 ——\n"
           "      所以必须把一次调用重复成千上万次，再用总时间除以次数。\n");
    return 0;
}
