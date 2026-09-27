// pipes.c —— 用 select() 同时监听多个管道
// 父进程 fork 出 3 个子进程，各自以不同间隔往自己的管道里写消息；
// 父进程是一个"事件循环"：select() 等任一管道可读，按就绪顺序读出并打印时间戳。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define N 3
static double t0;
static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0 - t0;
}

int main(void) {
    int interval[N] = {150, 250, 400};   // 子进程写消息的间隔（毫秒）
    int msgs[N] = {4, 3, 2};             // 每个子进程写几条
    int rfd[N];
    t0 = 0; t0 = now_ms();
    setvbuf(stdout, NULL, _IOLBF, 0);
    for (int k = 0; k < N; k++) {
        int p[2];
        if (pipe(p) < 0) { perror("pipe"); return 1; }
        pid_t pid = fork();
        if (pid == 0) {                  // 子进程：生产者
            close(p[0]);
            for (int i = 0; i < msgs[k]; i++) {
                usleep(interval[k] * 1000);
                char buf[64];
                int len = snprintf(buf, sizeof(buf), "child%d msg%d", k, i);
                if (write(p[1], buf, len) != len) exit(1);
            }
            close(p[1]);                 // 写端关闭 → 父进程读到 EOF
            exit(0);
        }
        close(p[1]);
        rfd[k] = p[0];
        printf("[parent] child%d 间隔 %dms 写 %d 条，读端 fd=%d\n", k, interval[k], msgs[k], rfd[k]);
    }
    int open_cnt = N;
    while (open_cnt > 0) {
        fd_set readFDs;
        FD_ZERO(&readFDs);
        int maxfd = -1;
        for (int k = 0; k < N; k++)
            if (rfd[k] >= 0) { FD_SET(rfd[k], &readFDs); if (rfd[k] > maxfd) maxfd = rfd[k]; }
        int rc = select(maxfd + 1, &readFDs, NULL, NULL, NULL);
        if (rc < 0) { perror("select"); return 1; }
        for (int k = 0; k < N; k++) {
            if (rfd[k] < 0 || !FD_ISSET(rfd[k], &readFDs)) continue;
            char buf[128];
            ssize_t n = read(rfd[k], buf, sizeof(buf) - 1);
            if (n <= 0) {
                printf("[%6.1fms] fd%d EOF（child%d 结束）\n", now_ms(), rfd[k], k);
                close(rfd[k]);
                rfd[k] = -1;
                open_cnt--;
            } else {
                buf[n] = '\0';
                printf("[%6.1fms] select rc=%d → fd%d 可读: \"%s\"\n", now_ms(), rc, rfd[k], buf);
            }
        }
    }
    while (wait(NULL) > 0) ;
    printf("[parent] 全部管道关闭，事件循环结束\n");
    return 0;
}
