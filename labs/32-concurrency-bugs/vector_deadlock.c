// vector_deadlock.c —— 复现第 32 章 Figure 32.6 式的死锁，并比较三种预防方法。
// 两个线程反复做 vector_add：线程 0 做 add(v0, v1)，线程 1 做 add(v1, v0)。
// 每次 add 都要同时持有"目标向量"和"源向量"两把锁。
//
// 用法: ./vector_deadlock <mode> [loops]
//   mode = naive   : 先锁 dst 再锁 src（两线程顺序相反 → 可能死锁）
//          ordered : 按锁地址排序加锁（循环等待被破坏）
//          trylock : lock(dst) + trylock(src)，失败就全部释放重来（非抢占被"绕开"）
//          global  : 先拿全局 prevention 锁，再一次性拿齐两把锁（持有并等待被破坏）
// 主线程是看门狗：若 1 秒内没有任何进展，就报告"疑似死锁"并退出，不会永远挂住。
#define _GNU_SOURCE
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define VLEN 64

typedef struct {
    pthread_mutex_t lock;
    int id;
    int values[VLEN];
} vector_t;

enum { NAIVE, ORDERED, TRYLOCK, GLOBAL };
static pthread_barrier_t start;  // 让两个线程同时起跑，保证真的交错执行

static const char *mode_names[] = { "naive", "ordered", "trylock", "global" };

static vector_t v[2];
static pthread_mutex_t prevention = PTHREAD_MUTEX_INITIALIZER;
static int mode;
static long loops = 100000;
static atomic_long progress;       // 完成的 vector_add 总次数
static atomic_long retries;        // trylock 失败次数
static atomic_int holding[2] = { -1, -1 };  // 线程 t 当前持有的第一把锁（向量编号）
static atomic_int waiting[2] = { -1, -1 };  // 线程 t 正在等待的锁（向量编号）

static void vector_add(int tid, vector_t *dst, vector_t *src) {
    switch (mode) {
    case NAIVE:
        pthread_mutex_lock(&dst->lock);
        atomic_store(&holding[tid], dst->id);
        atomic_store(&waiting[tid], src->id);
        pthread_mutex_lock(&src->lock);
        atomic_store(&waiting[tid], -1);
        break;
    case ORDERED:
        // 书中的 TIP：按锁的地址（高→低）排序，谁先谁后与参数顺序无关
        if (&dst->lock > &src->lock) {
            pthread_mutex_lock(&dst->lock);
            pthread_mutex_lock(&src->lock);
        } else {
            pthread_mutex_lock(&src->lock);
            pthread_mutex_lock(&dst->lock);
        }
        break;
    case TRYLOCK:
    top:
        pthread_mutex_lock(&dst->lock);
        if (pthread_mutex_trylock(&src->lock) != 0) {
            pthread_mutex_unlock(&dst->lock);
            atomic_fetch_add(&retries, 1);
            goto top;
        }
        break;
    case GLOBAL:
        pthread_mutex_lock(&prevention);   // 开始"原子地"获取全部锁
        pthread_mutex_lock(&dst->lock);
        pthread_mutex_lock(&src->lock);
        pthread_mutex_unlock(&prevention);
        break;
    }
    for (int i = 0; i < VLEN; i++)
        dst->values[i] += src->values[i];
    atomic_store(&holding[tid], -1);
    pthread_mutex_unlock(&src->lock);
    pthread_mutex_unlock(&dst->lock);
}

static double now(void);
static double finish[2];           // 每个线程完成的时刻（用来计时，不受看门狗轮询间隔影响）

static void *worker(void *arg) {
    int tid = (int)(long)arg;
    pthread_barrier_wait(&start);
    for (long i = 0; i < loops; i++) {
        if (tid == 0) vector_add(0, &v[0], &v[1]);
        else          vector_add(1, &v[1], &v[0]);
        atomic_fetch_add(&progress, 1);
    }
    finish[tid] = now();
    return NULL;
}

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s naive|ordered|trylock|global [loops]\n", argv[0]);
        return 1;
    }
    mode = -1;
    for (int m = 0; m < 4; m++)
        if (strcmp(argv[1], mode_names[m]) == 0) mode = m;
    if (mode < 0) { fprintf(stderr, "unknown mode %s\n", argv[1]); return 1; }
    if (argc > 2) loops = atol(argv[2]);

    for (int k = 0; k < 2; k++) {
        pthread_mutex_init(&v[k].lock, NULL);
        v[k].id = k;
        for (int i = 0; i < VLEN; i++) v[k].values[i] = 1;
    }

    pthread_barrier_init(&start, NULL, 2);
    double t0 = now();
    pthread_t th[2];
    for (long t = 0; t < 2; t++)
        pthread_create(&th[t], NULL, worker, (void *)t);

    // 看门狗：每 10ms 看一次进度，连续 100 次没变化就判定"疑似死锁"
    long last = -1;
    int still = 0;
    while (atomic_load(&progress) < 2 * loops) {
        usleep(10000);
        long p = atomic_load(&progress);
        if (p == last) {
            if (++still >= 100) {
                printf("[%s] 疑似死锁！已完成 %ld / %ld 次 vector_add，1 秒内毫无进展\n",
                       mode_names[mode], p, 2 * loops);
                for (int t = 0; t < 2; t++)
                    printf("  线程 %d: 持有 v%d 的锁，等待 v%d 的锁\n", t,
                           atomic_load(&holding[t]), atomic_load(&waiting[t]));
                printf("  等待图: T0 -> v%d -> T1 -> v%d -> T0  (出现环)\n",
                       atomic_load(&waiting[0]), atomic_load(&waiting[1]));
                exit(2);  // 两个线程永远醒不来，直接结束进程
            }
        } else {
            still = 0;
            last = p;
        }
    }
    for (int t = 0; t < 2; t++) pthread_join(th[t], NULL);
    double el = (finish[0] > finish[1] ? finish[0] : finish[1]) - t0;
    printf("[%s] 完成 %ld 次 vector_add，用时 %.3f 秒", mode_names[mode], 2 * loops, el);
    if (mode == TRYLOCK) printf("，trylock 失败重试 %ld 次", atomic_load(&retries));
    printf("\n");
    return 0;
}
