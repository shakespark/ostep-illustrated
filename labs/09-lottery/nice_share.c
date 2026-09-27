// nice_share.c —— 验证 CFS（及 Linux 6.6+ 的 EEVDF）按 nice 权重分配 CPU
// 用法: ./nice_share <nice1> <nice2> [秒数]
// 两个子进程被绑定到同一个 CPU（CPU 0），各自设置 nice 值后拼命做空循环，
// 时间到后通过管道把循环次数报告给父进程；父进程打印实际份额与权重表给出的理论份额。
#define _GNU_SOURCE
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

static const int prio_to_weight[40] = {
    /* -20 */ 88761, 71755, 56483, 46273, 36291,
    /* -15 */ 29154, 23254, 18705, 14949, 11916,
    /* -10 */ 9548,  7620,  6100,  4904,  3906,
    /*  -5 */ 3121,  2501,  1991,  1586,  1277,
    /*   0 */ 1024,  820,   655,   526,   423,
    /*   5 */ 335,   272,   215,   172,   137,
    /*  10 */ 110,   87,    70,    56,    45,
    /*  15 */ 36,    29,    23,    18,    15,
};

static volatile sig_atomic_t stop = 0;
static void on_alarm(int sig) { (void)sig; stop = 1; }

static void child(int nice_val, int secs, int fd) {
    if (setpriority(PRIO_PROCESS, 0, nice_val) != 0) {
        perror("setpriority (negative nice needs root)");
        exit(1);
    }
    signal(SIGALRM, on_alarm);
    // 两个子进程都在 fork 后才开始计时；各自计时 secs 秒，偏差可以忽略
    alarm(secs);
    unsigned long long loops = 0;
    while (!stop) loops++;
    if (write(fd, &loops, sizeof loops) != sizeof loops) exit(1);
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s <nice1> <nice2> [seconds]\n", argv[0]);
        return 1;
    }
    int nice_v[2] = { atoi(argv[1]), atoi(argv[2]) };
    int secs = argc > 3 ? atoi(argv[3]) : 3;
    for (int i = 0; i < 2; i++)
        if (nice_v[i] < -20 || nice_v[i] > 19) { fprintf(stderr, "nice must be in [-20, 19]\n"); return 1; }

    // 绑定到 CPU 0：子进程会继承这个亲和性，于是两者必须在同一个 CPU 上竞争
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(0, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0) { perror("sched_setaffinity"); return 1; }

    int pfd[2][2];
    pid_t pid[2];
    for (int i = 0; i < 2; i++) {
        if (pipe(pfd[i]) != 0) { perror("pipe"); return 1; }
        pid[i] = fork();
        if (pid[i] < 0) { perror("fork"); return 1; }
        if (pid[i] == 0) { close(pfd[i][0]); child(nice_v[i], secs, pfd[i][1]); }
        close(pfd[i][1]);
    }
    printf("both children pinned to CPU 0, running %d s\n", secs);

    unsigned long long loops[2] = { 0, 0 };
    for (int i = 0; i < 2; i++) {
        if (read(pfd[i][0], &loops[i], sizeof loops[i]) != sizeof loops[i]) { fprintf(stderr, "child %d failed\n", i + 1); return 1; }
        waitpid(pid[i], NULL, 0);
    }
    double total = (double)loops[0] + (double)loops[1];
    int w[2] = { prio_to_weight[nice_v[0] + 20], prio_to_weight[nice_v[1] + 20] };
    for (int i = 0; i < 2; i++)
        printf("child %d: nice %3d  weight %5d  loops %11llu  share %4.1f%%  (expected %4.1f%%)\n",
               i + 1, nice_v[i], w[i], loops[i], 100.0 * loops[i] / total, 100.0 * w[i] / (w[0] + w[1]));
    return 0;
}
