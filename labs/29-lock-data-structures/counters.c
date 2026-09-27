// counters.c —— 精确计数器（一把大锁） vs 近似计数器（sloppy counter）
// 复刻 OSTEP 第 29 章 Figure 29.5 / 29.6 的实验。
//
// 用法：
//   ./counters                    跑完整实验：线程数扫描 + 阈值 S 扫描
//   ./counters precise T N        T 个线程，每个线程 N 次 +1，精确计数器
//   ./counters approx  T N S      同上，近似计数器，阈值 S
//
// 编译：gcc -O2 -Wall -pthread -o counters counters.c
#define _GNU_SOURCE
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>

#define MAXCPUS 256

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

// ---------------- 精确计数器（Figure 29.2） ----------------
typedef struct {
    int value;
    pthread_mutex_t lock;
} precise_t;

static void precise_init(precise_t *c) {
    c->value = 0;
    assert(pthread_mutex_init(&c->lock, NULL) == 0);
}
static void precise_increment(precise_t *c) {
    pthread_mutex_lock(&c->lock);
    c->value++;
    pthread_mutex_unlock(&c->lock);
}
static int precise_get(precise_t *c) {
    pthread_mutex_lock(&c->lock);
    int rc = c->value;
    pthread_mutex_unlock(&c->lock);
    return rc;
}

// ---------------- 近似计数器（Figure 29.4） ----------------
// 与原书唯一的区别：每个 CPU 的 local/llock 放在各自独占的 64 字节缓存行里，
// 避免"伪共享"（false sharing）——否则不同 CPU 更新相邻的 local[i] 时
// 仍会互相抢同一条缓存行，扩展性会差很多。
#ifdef NOPAD   // make nopad：按原书写法紧挨着放，观察伪共享的代价
typedef struct {
    int value;
    pthread_mutex_t lock;
} local_t;
#else
typedef struct {
    int value;
    pthread_mutex_t lock;
} __attribute__((aligned(64))) local_t;
#endif

typedef struct {
    int global;               // 全局计数
    pthread_mutex_t glock;    // 全局锁
    local_t local[MAXCPUS];   // 每 CPU 局部计数 + 局部锁
    int threshold;            // 阈值 S
    int numcpus;
} approx_t;

static void approx_init(approx_t *c, int threshold, int numcpus) {
    c->threshold = threshold;
    c->numcpus = numcpus;
    c->global = 0;
    assert(pthread_mutex_init(&c->glock, NULL) == 0);
    for (int i = 0; i < numcpus; i++) {
        c->local[i].value = 0;
        assert(pthread_mutex_init(&c->local[i].lock, NULL) == 0);
    }
}
static void approx_update(approx_t *c, int threadID, int amt) {
    int cpu = threadID % c->numcpus;
    pthread_mutex_lock(&c->local[cpu].lock);
    c->local[cpu].value += amt;
    if (c->local[cpu].value >= c->threshold) {
        pthread_mutex_lock(&c->glock);          // 转移到全局
        c->global += c->local[cpu].value;
        pthread_mutex_unlock(&c->glock);
        c->local[cpu].value = 0;
    }
    pthread_mutex_unlock(&c->local[cpu].lock);
}
static int approx_get(approx_t *c) {           // 只读全局值：近似！
    pthread_mutex_lock(&c->glock);
    int val = c->global;
    pthread_mutex_unlock(&c->glock);
    return val;
}

// ---------------- 线程与计时 ----------------
static precise_t P;
static approx_t A;
static int loops;
static int use_approx;

typedef struct { int id; } arg_t;

static void *worker(void *arg) {
    int id = ((arg_t *)arg)->id;
    if (use_approx)
        for (int i = 0; i < loops; i++) approx_update(&A, id, 1);
    else
        for (int i = 0; i < loops; i++) precise_increment(&P);
    return NULL;
}

// 返回耗时（秒）；*final 返回 get() 读到的值
static double run(int approx, int nthreads, int nloops, int S, int numcpus, int *final) {
    pthread_t th[MAXCPUS];
    arg_t args[MAXCPUS];
    use_approx = approx;
    loops = nloops;
    if (approx) approx_init(&A, S, numcpus); else precise_init(&P);
    double t0 = now();
    for (int i = 0; i < nthreads; i++) {
        args[i].id = i;
        assert(pthread_create(&th[i], NULL, worker, &args[i]) == 0);
    }
    for (int i = 0; i < nthreads; i++) assert(pthread_join(th[i], NULL) == 0);
    double t1 = now();
    *final = approx ? approx_get(&A) : precise_get(&P);
    return t1 - t0;
}

// 同一配置跑 reps 次取中位数，减少噪声
static double median_run(int approx, int nthreads, int nloops, int S, int numcpus, int *final, int reps) {
    double t[16];
    for (int r = 0; r < reps; r++) t[r] = run(approx, nthreads, nloops, S, numcpus, final);
    for (int i = 0; i < reps; i++)
        for (int j = i + 1; j < reps; j++)
            if (t[j] < t[i]) { double x = t[i]; t[i] = t[j]; t[j] = x; }
    return t[reps / 2];
}

int main(int argc, char *argv[]) {
    int numcpus = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (numcpus > MAXCPUS) numcpus = MAXCPUS;
    int final;

    if (argc >= 4) {
        int approx = strcmp(argv[1], "approx") == 0;
        int T = atoi(argv[2]), N = atoi(argv[3]);
        int S = argc >= 5 ? atoi(argv[4]) : 1024;
        if (T < 1 || T > MAXCPUS) { fprintf(stderr, "threads must be 1..%d\n", MAXCPUS); return 1; }
        double sec = run(approx, T, N, S, numcpus, &final);
        printf("%s threads=%d loops=%d time=%.3fs get()=%d expected=%d\n",
               approx ? "approx" : "precise", T, N, sec, final, T * N);
        if (approx) printf("  (S=%d, NUMCPUS=%d; lag = %d)\n", S, numcpus, T * N - final);
        return 0;
    }

    const int N = 1000000, REPS = 3;
#ifdef NOPAD
    const char *tag = "[NOPAD 未填充缓存行] ";
#else
    const char *tag = "";
#endif
    printf("%sNUMCPUS (online) = %d, 每线程 %d 次递增, 每项取 %d 次运行的中位数\n\n", tag, numcpus, N, REPS);

    printf("== 实验一：线程数扫描（Figure 29.5 风格，近似计数器 S=1024）==\n");
    printf("%8s %12s %12s %14s %14s\n", "threads", "precise(s)", "approx(s)", "precise get()", "approx get()");
    int ts[] = {1, 2, 4, 8, 16};
    for (int k = 0; k < 5; k++) {
        int T = ts[k], fp, fa;
        double tp = median_run(0, T, N, 1024, numcpus, &fp, REPS);
        double ta = median_run(1, T, N, 1024, numcpus, &fa, REPS);
        printf("%8d %12.3f %12.3f %14d %14d\n", T, tp, ta, fp, fa);
        fflush(stdout);
    }

    printf("\n== 实验二：阈值 S 扫描（Figure 29.6 风格，4 线程）==\n");
    printf("%8s %12s %14s %12s\n", "S", "approx(s)", "approx get()", "lag");
    for (int S = 1; S <= 1024; S *= 2) {
        int fa;
        double ta = median_run(1, 4, N, S, numcpus, &fa, REPS);
        printf("%8d %12.3f %14d %12d\n", S, ta, fa, 4 * N - fa);
        fflush(stdout);
    }
    return 0;
}
