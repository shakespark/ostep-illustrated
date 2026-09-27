// cond_wait.c —— 用条件变量等待子线程完成（对比：用 flag 自旋等待）
#include "common_threads.h"
#include <time.h>
#include <unistd.h>

static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;   // 静态初始化
static pthread_cond_t  cond = PTHREAD_COND_INITIALIZER;
static int ready = 0;
static volatile int done_flag = 0;                          // 反面教材用的裸 flag

static double cpu_seconds(void) {                           // 本进程消耗的 CPU 时间（所有线程）
    struct timespec ts; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void *child_cv(void *arg) {
    (void) arg;
    usleep(500 * 1000);                    // 子线程"干活"0.5 秒（睡眠，不占 CPU）
    Pthread_mutex_lock(&lock);
    ready = 1;                             // 修改条件时持有锁
    printf("  [子线程] ready = 1，signal\n");
    Pthread_cond_signal(&cond);
    Pthread_mutex_unlock(&lock);
    return NULL;
}

void *child_spin(void *arg) {
    (void) arg;
    usleep(500 * 1000);
    done_flag = 1;                         // 没有锁、没有 cond：原书说 "Don't ever do this"
    return NULL;
}

int main(void) {
    pthread_t p;
    double t0 = cpu_seconds();
    printf("== 条件变量等待 ==\n");
    Pthread_create(&p, NULL, child_cv, NULL);
    Pthread_mutex_lock(&lock);
    int wakeups = 0;
    while (ready == 0) {                   // 一定是 while：醒来后重新检查条件
        Pthread_cond_wait(&cond, &lock);   // 睡眠时释放 lock，返回前重新获得 lock
        wakeups++;
    }
    Pthread_mutex_unlock(&lock);
    Pthread_join(p, NULL);
    double t1 = cpu_seconds();
    printf("main: 等到了 ready（被唤醒 %d 次），等待期间消耗 CPU %.3f 秒\n", wakeups, t1 - t0);

    printf("== 反面教材：自旋等待 flag ==\n");
    Pthread_create(&p, NULL, child_spin, NULL);
    long spins = 0;
    while (done_flag == 0) spins++;        // 空转，白白烧 CPU
    Pthread_join(p, NULL);
    double t2 = cpu_seconds();
    printf("main: 等到了 flag（空转 %ld 次），等待期间消耗 CPU %.3f 秒\n", spins, t2 - t1);
    return 0;
}
