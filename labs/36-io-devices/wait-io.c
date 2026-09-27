// wait-io.c —— 用"慢设备"(一个延迟后才写管道的子进程)对比三种等待方式的 CPU 消耗：
//   poll   : 非阻塞 read 反复自旋（相当于轮询设备状态寄存器）
//   block  : 阻塞 read，进程睡眠，数据到了由内核唤醒（相当于中断）
//   hybrid : 先自旋 spin_us 微秒，还没好再阻塞（书中的"两阶段"做法）
// 用法: ./wait-io <poll|block|hybrid> [设备延迟毫秒, 默认 500] [hybrid 自旋微秒, 默认 100]
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double now_ms(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1e3 + t.tv_nsec / 1e6;
}
static double tv_ms(struct timeval t) { return t.tv_sec * 1e3 + t.tv_usec / 1e3; }

int main(int argc, char *argv[]) {
    const char *mode = argc > 1 ? argv[1] : "poll";
    int delay_ms = argc > 2 ? atoi(argv[2]) : 500;
    int spin_us = argc > 3 ? atoi(argv[3]) : 100;
    if (strcmp(mode, "poll") && strcmp(mode, "block") && strcmp(mode, "hybrid")) {
        fprintf(stderr, "usage: %s <poll|block|hybrid> [delay_ms] [spin_us]\n", argv[0]);
        return 1;
    }
    int p[2];
    if (pipe(p) < 0) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid == 0) {                       // 子进程 = "设备"
        close(p[0]);
        usleep(delay_ms * 1000);          // 设备忙：处理请求需要 delay_ms
        char c = 'D';
        write(p[1], &c, 1);               // 完成：数据就绪（相当于设备置 READY）
        _exit(0);
    }
    close(p[1]);

    struct rusage r0, r1;
    getrusage(RUSAGE_SELF, &r0);
    double t0 = now_ms();
    long polls = 0;
    char c;

    if (strcmp(mode, "block") == 0) {
        read(p[0], &c, 1);                // 睡眠，直到数据到来才被唤醒
    } else {
        fcntl(p[0], F_SETFL, O_NONBLOCK); // 变成"只看一眼状态"
        double spin_until = now_ms() + spin_us / 1000.0;
        for (;;) {
            ssize_t n = read(p[0], &c, 1);
            polls++;
            if (n == 1) break;            // STATUS != BUSY
            if (n < 0 && errno != EAGAIN) { perror("read"); return 1; }
            if (strcmp(mode, "hybrid") == 0 && now_ms() > spin_until) {
                fcntl(p[0], F_SETFL, 0);  // 自旋够久了：改为阻塞等待
                read(p[0], &c, 1);
                break;
            }
        }
    }

    double wall = now_ms() - t0;
    getrusage(RUSAGE_SELF, &r1);
    double user = tv_ms(r1.ru_utime) - tv_ms(r0.ru_utime);
    double sys = tv_ms(r1.ru_stime) - tv_ms(r0.ru_stime);
    waitpid(pid, NULL, 0);

    printf("mode=%-6s device=%dms  wall=%7.1f ms  cpu(user+sys)=%7.1f ms  (%5.1f%% of wall)  polls=%ld\n",
           mode, delay_ms, wall, user + sys, 100.0 * (user + sys) / wall, polls);
    return 0;
}
