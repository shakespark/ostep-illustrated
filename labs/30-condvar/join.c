// join.c —— 用条件变量实现 thr_join()，以及原书两个错误版本
//   ./join 0 : 正确版（状态变量 done + 锁 + while），原书 Figure 30.3
//   ./join 1 : 没有状态变量 done（Figure 30.4），并让子线程先跑完 → 信号丢失
//   ./join 2 : 不持锁（Figure 30.5），在"检查 done"与"wait"之间制造一个窗口 → 信号丢失
// 为了不让程序真的永远挂住，父线程用 pthread_cond_timedwait 等最多 2 秒。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

static int done = 0, mode = 0;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t dummy = PTHREAD_MUTEX_INITIALIZER;   // mode 2 里假装"不需要锁"
static pthread_cond_t c = PTHREAD_COND_INITIALIZER;

static int wait2s(pthread_cond_t *cv, pthread_mutex_t *mx) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;
    return pthread_cond_timedwait(cv, mx, &ts);
}

static void thr_exit(void) {
    if (mode == 0) {
        pthread_mutex_lock(&m); done = 1; pthread_cond_signal(&c); pthread_mutex_unlock(&m);
    } else if (mode == 1) {
        pthread_mutex_lock(&m); pthread_cond_signal(&c); pthread_mutex_unlock(&m);
    } else {
        done = 1; pthread_cond_signal(&c);                 // 不持锁
    }
}

static void *child(void *arg) {
    (void)arg;
    printf("child\n");
    thr_exit();
    printf("child: 已 signal（此刻有没有人在等？）\n");
    return NULL;
}

static void thr_join(void) {
    int rc = 0;
    if (mode == 0) {
        pthread_mutex_lock(&m);
        while (done == 0 && rc == 0) rc = wait2s(&c, &m);
        pthread_mutex_unlock(&m);
    } else if (mode == 1) {
        usleep(100000);                                    // 让子线程先跑完
        pthread_mutex_lock(&m);
        rc = wait2s(&c, &m);                               // 没有状态变量，只能盲等
        pthread_mutex_unlock(&m);
    } else {
        if (done == 0) {                                   // 检查……
            usleep(100000);                                // ……被"中断"，子线程趁机跑完
            pthread_mutex_lock(&dummy);
            rc = wait2s(&c, &dummy);                       // ……然后才去睡
            pthread_mutex_unlock(&dummy);
        }
    }
    if (rc == ETIMEDOUT)
        printf("parent: 等了 2 秒没人叫醒 —— 信号丢失，真实程序会永远睡下去！\n");
}

int main(int argc, char *argv[]) {
    mode = argc > 1 ? atoi(argv[1]) : 0;
    if (mode < 0 || mode > 2) { fprintf(stderr, "用法: %s [0|1|2]\n", argv[0]); return 1; }
    const char *name[] = {"正确版：done + 锁 + while", "错误版：没有状态变量 done", "错误版：不持锁"};
    printf("== 模式 %d：%s ==\n", mode, name[mode]);
    printf("parent: begin\n");
    pthread_t p;
    pthread_create(&p, NULL, child, NULL);
    thr_join();
    printf("parent: end\n");
    pthread_join(p, NULL);
    return 0;
}
