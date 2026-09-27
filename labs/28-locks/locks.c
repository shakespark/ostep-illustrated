// locks.c：用 GCC 原子内建实现几种锁，和 pthread_mutex 比较"多线程计数"的正确性与耗时。
//
// 用法：./locks <锁类型> <线程数> <每线程循环次数>
//   锁类型：none   不加锁（对照组，会出错）
//           flag   原书 Figure 28.1 的错误锁：普通 load/store 的 flag
//           tas    test-and-set 自旋锁（__sync_lock_test_and_set，x86 上是 xchg）
//           ttas   test-and-test-and-set：先普通读到 0 再 TAS，减少总线争用
//           cas    compare-and-swap 自旋锁（__sync_val_compare_and_swap，x86 上是 lock cmpxchg）
//           ticket ticket 锁（__atomic_fetch_add，x86 上是 lock xadd）
//           yield  TAS + sched_yield()（原书 Figure 28.8）
//           mutex  pthread_mutex_t（glibc：先原子操作，抢不到才 futex 睡眠）
// 可选第 4 个参数：时间上限（秒，默认 20）。超时则打印已完成的次数和吞吐量后退出，
// 用来观察 ticket 锁在"线程数 > CPU 数"时的性能崩溃，而不必真的等几个小时。
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <unistd.h>

static volatile long counter = 0;
static long loops;

// ---------- Figure 28.1：只用 load/store 的"锁"（错误！） ----------
static volatile int flag_lock = 0;
static void flag_acquire(void) {
    while (flag_lock == 1)
        ;                    // spin
    flag_lock = 1;           // 检查与设置之间可以被打断 → 两个线程都进入
}
static void flag_release(void) { flag_lock = 0; }

// ---------- Figure 28.3：test-and-set 自旋锁 ----------
static volatile int tas_flag = 0;
static void tas_acquire(void) {
    while (__sync_lock_test_and_set(&tas_flag, 1) == 1)
        ;                    // spin
}
static void tas_release(void) { __sync_lock_release(&tas_flag); }  // 带 release 语义的写 0

// ---------- test-and-test-and-set（homework 里的 test-and-test-and-set.s） ----------
static void ttas_acquire(void) {
    for (;;) {
        while (tas_flag == 1)
            __builtin_ia32_pause();   // 只读自旋：命中本地缓存，不抢总线
        if (__sync_lock_test_and_set(&tas_flag, 1) == 0)
            return;
    }
}

// ---------- Figure 28.4：compare-and-swap 自旋锁 ----------
static volatile int cas_flag = 0;
static void cas_acquire(void) {
    while (__sync_val_compare_and_swap(&cas_flag, 0, 1) == 1)
        ;
}
static void cas_release(void) { __sync_lock_release(&cas_flag); }

// ---------- Figure 28.7：ticket 锁（fetch-and-add） ----------
static volatile int ticket = 0, turn = 0;
static void ticket_acquire(void) {
    int myturn = __atomic_fetch_add(&ticket, 1, __ATOMIC_RELAXED);
    while (__atomic_load_n(&turn, __ATOMIC_ACQUIRE) != myturn)
        ;                    // spin
}
static void ticket_release(void) {
    __atomic_store_n(&turn, turn + 1, __ATOMIC_RELEASE);   // 只有持锁者写 turn
}

// ---------- Figure 28.8：test-and-set + yield ----------
static void yield_acquire(void) {
    while (__sync_lock_test_and_set(&tas_flag, 1) == 1)
        sched_yield();       // 抢不到就让出 CPU
}

// ---------- pthread_mutex ----------
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static void mutex_acquire(void) { pthread_mutex_lock(&mutex); }
static void mutex_release(void) { pthread_mutex_unlock(&mutex); }

static void nop(void) {}

typedef struct { const char *name; void (*acq)(void); void (*rel)(void); } lock_ops;
static lock_ops table[] = {
    {"none",   nop,            nop},
    {"flag",   flag_acquire,   flag_release},
    {"tas",    tas_acquire,    tas_release},
    {"ttas",   ttas_acquire,   tas_release},
    {"cas",    cas_acquire,    cas_release},
    {"ticket", ticket_acquire, ticket_release},
    {"yield",  yield_acquire,  tas_release},
    {"mutex",  mutex_acquire,  mutex_release},
};
static lock_ops *ops;
static pthread_barrier_t start;
static int finished = 0;          // 已完成的线程数（main 用来实现超时）   // 所有线程就位后同时开跑，保证真的有竞争

static void *worker(void *arg) {
    (void) arg;
    pthread_barrier_wait(&start);
    for (long i = 0; i < loops; i++) {
        ops->acq();
        counter = counter + 1;          // 临界区
        ops->rel();
    }
    __atomic_fetch_add(&finished, 1, __ATOMIC_RELEASE);
    return NULL;
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc != 4 && argc != 5) {
        fprintf(stderr, "usage: %s none|flag|tas|ttas|cas|ticket|yield|mutex <threads> <loops> [limit_sec]\n", argv[0]);
        return 1;
    }
    for (size_t k = 0; k < sizeof(table) / sizeof(table[0]); k++)
        if (strcmp(argv[1], table[k].name) == 0) ops = &table[k];
    if (!ops) { fprintf(stderr, "unknown lock type %s\n", argv[1]); return 1; }
    int n = atoi(argv[2]);
    loops = atol(argv[3]);

    pthread_t *t = malloc(sizeof(pthread_t) * n);
    pthread_barrier_init(&start, NULL, n);
    double t0 = now();
    for (int i = 0; i < n; i++)
        if (pthread_create(&t[i], NULL, worker, NULL) != 0) { perror("pthread_create"); return 1; }
    double limit = argc == 5 ? atof(argv[4]) : 20.0;
    while (__atomic_load_n(&finished, __ATOMIC_ACQUIRE) < n) {
        struct timespec ts = {0, 1000000};       // 每 1ms 看一眼
        nanosleep(&ts, NULL);
        if (now() - t0 > limit) {
            long c = counter;
            printf("%-6s threads=%-3d TIMEOUT after %.1fs: only %ld of %ld increments done (%.0f per second)\n",
                   ops->name, n, limit, c, (long) n * loops, c / limit);
            fflush(stdout);
            _exit(2);
        }
    }
    for (int i = 0; i < n; i++)
        pthread_join(t[i], NULL);
    double el = now() - t0;

    long expect = (long) n * loops;
    printf("%-6s threads=%-3d counter=%-9ld expected=%-9ld %-5s time=%.3fs\n",
           ops->name, n, counter, expect, counter == expect ? "OK" : "WRONG", el);
    free(t);
    return 0;
}
