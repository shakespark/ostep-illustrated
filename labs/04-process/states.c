// states.c —— 让同一时刻存在处于不同状态的进程，再用 ps 看它们的 STAT 列
//   R = Running/Runnable（运行或就绪）  S = Sleeping（阻塞，等待事件）
//   T = Stopped（被 SIGSTOP 暂停）      Z = Zombie（已退出、尚未被父进程 wait 回收）
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>

static void spin(double sec) {           // 纯 CPU 计算：一直处于 R
    struct timespec a, b;
    clock_gettime(CLOCK_MONOTONIC, &a);
    do { clock_gettime(CLOCK_MONOTONIC, &b); }
    while ((b.tv_sec - a.tv_sec) + (b.tv_nsec - a.tv_nsec) / 1e9 < sec);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    pid_t kids[4];
    const char *what[4] = { "spin  (CPU 计算)", "sleep (等待定时器)", "stop  (收到 SIGSTOP)", "exit  (立即退出)" };

    for (int i = 0; i < 4; i++) {
        pid_t rc = fork();
        if (rc < 0) { perror("fork"); exit(1); }
        if (rc == 0) {
            switch (i) {
            case 0: spin(2.0); break;             // → R
            case 1: sleep(2);  break;             // → S（阻塞）
            case 2: raise(SIGSTOP); break;        // → T（暂停），等父进程 SIGCONT
            case 3: break;                        // → 立即退出，变成 Z
            }
            exit(0);
        }
        kids[i] = rc;
    }
    usleep(300 * 1000);                           // 让孩子们各就各位
    printf("父进程 %d 创建了 4 个子进程：\n", getpid());
    for (int i = 0; i < 4; i++) printf("  %d  %s\n", kids[i], what[i]);

    char cmd[128];
    snprintf(cmd, sizeof cmd, "ps -o pid,ppid,stat,cmd --ppid %d", getpid());
    printf("\n$ %s\n", cmd);
    system(cmd);

    kill(kids[2], SIGCONT);                       // 恢复被暂停的进程（Miscellaneous Control）
    for (int i = 0; i < 4; i++) {                 // wait 回收：僵尸就此消失
        int st;
        pid_t w = waitpid(kids[i], &st, 0);
        printf("waitpid 回收了 %d，退出码 %d\n", w, WEXITSTATUS(st));
    }
    printf("\n回收之后再看一次（应该一个子进程都没有了）：\n$ %s\n", cmd);
    system(cmd);
    return 0;
}
