// syscall.c —— 测量系统调用的开销：反复做"读 0 个字节"的 read()，总时间 / 次数
// 对照组：普通函数调用（不陷入内核）、clock_gettime（Linux 用 vDSO 在用户态完成，也不陷入内核）
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <sys/syscall.h>

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

__attribute__((noinline)) static int plain_call(int x) {
    __asm__ volatile("" ::: "memory");       // 阻止编译器把函数调用优化掉
    return x + 1;
}

int main(int argc, char *argv[]) {
    long n = argc > 1 ? atol(argv[1]) : 2000000;
    char buf[1];
    int fd = open("/dev/zero", O_RDONLY);
    double t0, t1;

    t0 = now();
    volatile int sink = 0;
    for (long i = 0; i < n; i++) sink = plain_call(sink);
    t1 = now();
    printf("普通函数调用            : %7.1f ns/次\n", (t1 - t0) / n * 1e9);

    t0 = now();
    struct timespec ts;
    for (long i = 0; i < n; i++) clock_gettime(CLOCK_MONOTONIC, &ts);
    t1 = now();
    printf("clock_gettime (vDSO)    : %7.1f ns/次  ← 看起来像系统调用，其实没进内核\n", (t1 - t0) / n * 1e9);

    t0 = now();
    for (long i = 0; i < n; i++) syscall(SYS_getppid);
    t1 = now();
    printf("getppid()   系统调用    : %7.1f ns/次\n", (t1 - t0) / n * 1e9);

    t0 = now();
    for (long i = 0; i < n; i++) read(fd, buf, 0);
    t1 = now();
    printf("read(fd,buf,0) 系统调用 : %7.1f ns/次  (重复 %ld 次)\n", (t1 - t0) / n * 1e9, n);
    close(fd);
    return 0;
}
