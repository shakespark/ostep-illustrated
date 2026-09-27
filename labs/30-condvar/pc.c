// pc.c —— 生产者/消费者（有界缓冲区）三个版本对照
//   -v 1 : if    + 单个条件变量（原书 Figure 30.8，Broken v1）
//   -v 2 : while + 单个条件变量（原书 Figure 30.10，Broken v2）
//   -v 3 : while + 两个条件变量 empty/fill（原书 Figure 30.14，正确）
// 其他参数：-p 生产者数 -c 消费者数 -m 缓冲区大小 MAX -l 每个生产者生产的个数
// 消费者总共要取走 p*l 个数据。看门狗线程：如果 2 秒内没有任何进展，
// 就判定"所有线程都睡着了"（原书 Figure 30.11 的情形）并退出。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>

static int *buffer, MAX = 1, fill_ptr = 0, use_ptr = 0, count = 0;
static int version = 3, loops = 10000, np = 1, nc = 2;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond = PTHREAD_COND_INITIALIZER;    // v1/v2 共用的唯一条件变量
static pthread_cond_t empty = PTHREAD_COND_INITIALIZER;   // v3：生产者等它
static pthread_cond_t fill = PTHREAD_COND_INITIALIZER;    // v3：消费者等它
static volatile long progress = 0;      // 每 put/get 一次就 +1，看门狗据此判断是否卡住
static long consumed_total = 0, target = 0;
static long wakeups = 0, wasted = 0;    // 从 wait 返回的次数 / 返回后发现条件仍不满足的次数

static void put(int v) {
    if (count == MAX) { fprintf(stderr, "断言失败：put() 时缓冲区已满 (count=%d)\n", count); exit(2); }
    buffer[fill_ptr] = v; fill_ptr = (fill_ptr + 1) % MAX; count++;
}
static int get(void) {
    if (count == 0) { fprintf(stderr, "断言失败：get() 时缓冲区为空 —— 这就是 Figure 30.9 的错误！\n"); exit(2); }
    int t = buffer[use_ptr]; use_ptr = (use_ptr + 1) % MAX; count--; return t;
}

static void *producer(void *arg) {
    (void)arg;
    for (int i = 0; i < loops; i++) {
        pthread_mutex_lock(&mutex);                                   // p1
        if (version == 1) {
            if (count == MAX) {                                       // p2 (if)
                pthread_cond_wait(&cond, &mutex);                     // p3
                wakeups++; if (count == MAX) wasted++;
            }
        } else {
            pthread_cond_t *cv = version == 2 ? &cond : &empty;
            while (count == MAX) {                                    // p2 (while)
                pthread_cond_wait(cv, &mutex);                        // p3
                wakeups++; if (count == MAX) wasted++;
            }
        }
        put(i);                                                       // p4
        pthread_cond_signal(version == 3 ? &fill : &cond);            // p5
        progress++;
        pthread_mutex_unlock(&mutex);                                 // p6
    }
    return NULL;
}

static void *consumer(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&mutex);                                   // c1
        if (consumed_total >= target) { pthread_mutex_unlock(&mutex); return NULL; }
        if (version == 1) {
            if (count == 0) {                                         // c2 (if)
                pthread_cond_wait(&cond, &mutex);                     // c3
                wakeups++; if (count == 0) wasted++;
                if (consumed_total >= target) { pthread_mutex_unlock(&mutex); return NULL; }
            }
        } else {
            pthread_cond_t *cv = version == 2 ? &cond : &fill;
            while (count == 0) {                                      // c2 (while)
                if (consumed_total >= target) { pthread_mutex_unlock(&mutex); return NULL; }
                pthread_cond_wait(cv, &mutex);                        // c3
                wakeups++; if (count == 0) wasted++;
            }
        }
        get();                                                        // c4
        consumed_total++;
        pthread_cond_signal(version == 3 ? &empty : &cond);           // c5
        if (consumed_total >= target) {               // 收尾：叫醒其他消费者让它们退出
            pthread_cond_broadcast(&cond); pthread_cond_broadcast(&fill);
        }
        progress++;
        pthread_mutex_unlock(&mutex);                                 // c6
    }
}

static void *watchdog(void *arg) {
    (void)arg;
    long last = -1;
    for (;;) {
        sleep(2);
        if (progress == last) {
            pthread_mutex_lock(&mutex);
            printf("看门狗：2 秒内没有任何 put/get！count=%d，已消费 %ld/%ld\n", count, consumed_total, target);
            printf("=> 所有线程都睡着了（原书 Figure 30.11 的情形）\n");
            printf("   从 wait 返回 %ld 次，其中 %ld 次白醒（条件仍不满足）\n", wakeups, wasted);
            fflush(stdout);
            _exit(3);
        }
        last = progress;
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "v:p:c:m:l:")) != -1) {
        switch (opt) {
        case 'v': version = atoi(optarg); break;
        case 'p': np = atoi(optarg); break;
        case 'c': nc = atoi(optarg); break;
        case 'm': MAX = atoi(optarg); break;
        case 'l': loops = atoi(optarg); break;
        default:
            fprintf(stderr, "用法: %s [-v 1|2|3] [-p 生产者] [-c 消费者] [-m MAX] [-l 每个生产者的个数]\n", argv[0]);
            return 1;
        }
    }
    if (version < 1 || version > 3 || np < 1 || nc < 1 || MAX < 1 || loops < 1 || np > 64 || nc > 64) {
        fprintf(stderr, "参数不合法\n"); return 1;
    }
    buffer = calloc(MAX, sizeof(int));
    target = (long)np * loops;
    const char *name[] = {"", "v1: if + 单 CV", "v2: while + 单 CV", "v3: while + empty/fill 两个 CV"};
    printf("%s，生产者 %d，消费者 %d，MAX=%d，共 %ld 个数据\n", name[version], np, nc, MAX, target);
    fflush(stdout);

    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    pthread_t wd, p[64], c[64];
    pthread_create(&wd, NULL, watchdog, NULL);
    for (int i = 0; i < np; i++) pthread_create(&p[i], NULL, producer, NULL);
    for (int i = 0; i < nc; i++) pthread_create(&c[i], NULL, consumer, NULL);
    for (int i = 0; i < np; i++) pthread_join(p[i], NULL);
    for (int i = 0; i < nc; i++) pthread_join(c[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
    printf("完成：消费 %ld 个，用时 %.1f ms；从 wait 返回 %ld 次，其中 %ld 次白醒\n", consumed_total, ms, wakeups, wasted);
    return 0;
}
