// ctxsw.c —— 仿照 lmbench 测上下文切换开销：两个进程用两根管道打乒乓
//   父：write(p1) → read(p2) 阻塞 → 切换到子
//   子：read(p1)  → write(p2) → read(p1) 阻塞 → 切换回父
// 每一个来回 = 2 次上下文切换 + 4 次管道系统调用。
// 用 sched_setaffinity 把两个进程绑在同一个 CPU 上，确保真的发生"停下一个、换上另一个"。
// 用法：./ctxsw [来回次数] [CPU编号，-1 表示不绑定]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <sys/wait.h>

static double now(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}

static void pin(int cpu) {
    if (cpu < 0) return;
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0) { perror("sched_setaffinity"); exit(1); }
}

int main(int argc, char *argv[]) {
    long n = argc > 1 ? atol(argv[1]) : 200000;
    int cpu = argc > 2 ? atoi(argv[2]) : 0;
    char c = 'x';

    // 基线：同一个进程里 write 再 read 同一根管道（不会阻塞，也就没有切换），测 4 次管道调用的成本
    int q[2];
    if (pipe(q) < 0) { perror("pipe"); return 1; }
    pin(cpu);
    double b0 = now();
    for (long i = 0; i < n; i++) {
        write(q[1], &c, 1); read(q[0], &c, 1);
        write(q[1], &c, 1); read(q[0], &c, 1);
    }
    double b1 = now();
    double base = (b1 - b0) / n;                 // 每轮 4 次管道调用的时间

    int p1[2], p2[2];
    if (pipe(p1) < 0 || pipe(p2) < 0) { perror("pipe"); return 1; }
    pid_t rc = fork();
    if (rc == 0) {                               // 子进程（继承了父进程的 CPU 绑定）
        for (long i = 0; i < n; i++) {
            read(p1[0], &c, 1);
            write(p2[1], &c, 1);
        }
        exit(0);
    }
    double t0 = now();
    for (long i = 0; i < n; i++) {
        write(p1[1], &c, 1);
        read(p2[0], &c, 1);
    }
    double t1 = now();
    wait(NULL);

    double round = (t1 - t0) / n;
    if (cpu < 0) printf("绑定 CPU        : 不绑定（两个进程可能在不同 CPU 上）\n");
    else         printf("绑定 CPU        : 父子进程都绑在 CPU %d 上\n", cpu);
    printf("来回次数        : %ld\n", n);
    printf("每个来回        : %7.2f us（2 次切换 + 4 次管道读写）\n", round * 1e6);
    printf("管道读写基线    : %7.2f us（同一进程内 4 次管道读写，无切换）\n", base * 1e6);
    printf("估算单次上下文切换: %5.2f us = (来回 - 基线) / 2\n", (round - base) / 2 * 1e6);
    return 0;
}
