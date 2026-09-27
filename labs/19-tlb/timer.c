// timer.c —— 作业第 1 题：计时器有多精确？连续调用 clock_gettime / gettimeofday，看两次读数的最小差值
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <time.h>

int main(void) {
    struct timespec res, a, b;
    clock_getres(CLOCK_MONOTONIC, &res);
    printf("clock_getres(CLOCK_MONOTONIC) = %ld ns\n", res.tv_nsec);

    long min_ns = 1000000000L, sum = 0;
    const int N = 1000000;
    for (int i = 0; i < N; i++) {
        clock_gettime(CLOCK_MONOTONIC, &a);
        clock_gettime(CLOCK_MONOTONIC, &b);
        long d = (b.tv_sec - a.tv_sec) * 1000000000L + (b.tv_nsec - a.tv_nsec);
        if (d < min_ns) min_ns = d;
        sum += d;
    }
    printf("clock_gettime 连续两次读数：最小差 %ld ns，平均差 %.1f ns\n", min_ns, (double)sum / N);

    struct timeval u1, u2;
    long umin = 1000000000L;
    for (int i = 0; i < N; i++) {
        gettimeofday(&u1, NULL);
        do { gettimeofday(&u2, NULL); } while (u2.tv_usec == u1.tv_usec && u2.tv_sec == u1.tv_sec);
        long d = (u2.tv_sec - u1.tv_sec) * 1000000L + (u2.tv_usec - u1.tv_usec);
        if (d < umin) umin = d;
        if (i > 1000) break;
    }
    printf("gettimeofday 能分辨的最小变化：%ld us（= %ld ns）\n", umin, umin * 1000);
    printf("结论：单次页访问只有几纳秒，必须把它重复成百万次，让总时间达到毫秒级再求平均。\n");
    return 0;
}
