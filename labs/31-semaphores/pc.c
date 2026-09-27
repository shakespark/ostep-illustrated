// pc.c —— 用信号量解决生产者/消费者（有界缓冲区）问题
//   ./pc              正确版（原书 Figure 31.12：mutex 只包住 put/get）
//   ./pc -b           错误版（原书 Figure 31.11：mutex 包在 empty/full 外面）→ 死锁
// 其他参数：-p 生产者数 -c 消费者数 -m 缓冲区大小 MAX -l 每个生产者生产的个数
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define MAXBUF 64
int buffer[MAXBUF];
int fill = 0, use = 0, MAX = 1, loops = 10000, broken = 0;
sem_t empty, full, mutex;
volatile long progress = 0;          // 看门狗用：每 put/get 一次 +1
volatile int where[16];              // 每个线程当前停在哪一行（仅用于死锁报告）
const char *names[] = {"运行中", "等 mutex", "等 empty", "等 full", "持有 mutex 并等 empty", "持有 mutex 并等 full", "结束"};

void put(int v) { buffer[fill] = v; fill = (fill + 1) % MAX; }
int  get(void)  { int t = buffer[use]; use = (use + 1) % MAX; return t; }

typedef struct { int id; long sum; long cnt; } arg_t;

void *producer(void *a) {
    arg_t *me = a;
    for (int i = 0; i < loops; i++) {
        int v = me->id * 1000000 + i;
        if (broken) {
            where[me->id] = 1; sem_wait(&mutex);            // P0
            where[me->id] = 4; sem_wait(&empty);            // P1
            put(v); progress++;                             // P2
            sem_post(&full);                                // P3
            sem_post(&mutex);                               // P4
        } else {
            where[me->id] = 2; sem_wait(&empty);            // P1
            where[me->id] = 1; sem_wait(&mutex);            // P1.5
            put(v); progress++;                             // P2
            sem_post(&mutex);                               // P2.5
            sem_post(&full);                                // P3
        }
        where[me->id] = 0;
        me->sum += v; me->cnt++;
    }
    where[me->id] = 6;
    return NULL;
}

void *consumer(void *a) {
    arg_t *me = a;
    int tmp = 0;
    while (1) {
        if (broken) {
            where[me->id] = 1; sem_wait(&mutex);            // C0
            where[me->id] = 5; sem_wait(&full);             // C1
            tmp = get(); progress++;                        // C2
            sem_post(&empty);                               // C3
            sem_post(&mutex);                               // C4
        } else {
            where[me->id] = 3; sem_wait(&full);             // C1
            where[me->id] = 1; sem_wait(&mutex);            // C1.5
            tmp = get(); progress++;                        // C2
            sem_post(&mutex);                               // C2.5
            sem_post(&empty);                               // C3
        }
        where[me->id] = 0;
        if (tmp == -1) break;                               // 结束标记
        me->sum += tmp; me->cnt++;
    }
    where[me->id] = 6;
    return NULL;
}

int main(int argc, char *argv[]) {
    int np = 1, nc = 1, opt;
    while ((opt = getopt(argc, argv, "p:c:m:l:b")) != -1) {
        switch (opt) {
        case 'p': np = atoi(optarg); break;
        case 'c': nc = atoi(optarg); break;
        case 'm': MAX = atoi(optarg); break;
        case 'l': loops = atoi(optarg); break;
        case 'b': broken = 1; break;
        default: fprintf(stderr, "用法: %s [-p N] [-c N] [-m MAX] [-l loops] [-b]\n", argv[0]); return 1;
        }
    }
    if (MAX < 1 || MAX > MAXBUF || np + nc > 16 || np < 1 || nc < 1) { fprintf(stderr, "参数超出范围\n"); return 1; }
    sem_init(&empty, 0, MAX);   // MAX 个空位
    sem_init(&full, 0, 0);      // 0 个满位
    sem_init(&mutex, 0, 1);     // 二值信号量当锁
    printf("%s版：%d 个生产者，%d 个消费者，MAX=%d，每个生产者 %d 个\n",
           broken ? "错误（mutex 在外层）" : "正确", np, nc, MAX, loops);

    pthread_t th[16]; arg_t args[16] = {{0}};
    // 先启动消费者，让它们先跑（这正是原书死锁场景：消费者先拿到 mutex）
    for (int j = 0; j < nc; j++) { args[np + j].id = np + j; pthread_create(&th[np + j], NULL, consumer, &args[np + j]); }
    usleep(100000);
    for (int i = 0; i < np; i++) { args[i].id = i; pthread_create(&th[i], NULL, producer, &args[i]); }

    // 看门狗：2 秒内没有任何 put/get 进展，就认为死锁
    long last = -1; int stall = 0, done = 0;
    while (!done) {
        usleep(100000);
        done = 1;
        for (int i = 0; i < np; i++) if (where[i] != 6) done = 0;
        if (done) break;
        if (progress == last) { if (++stall >= 20) {
            printf("!! 2 秒没有进展：检测到死锁。各线程状态：\n");
            for (int i = 0; i < np; i++) printf("   生产者 P%d: %s\n", i, names[where[i]]);
            for (int j = 0; j < nc; j++) printf("   消费者 C%d: %s\n", j, names[where[np + j]]);
            printf("   已完成 put/get 次数 = %ld\n", progress);
            return 2; } }
        else { stall = 0; last = progress; }
    }
    for (int i = 0; i < np; i++) pthread_join(th[i], NULL);
    // 放入 nc 个结束标记 -1（走正常的生产者协议）
    for (int j = 0; j < nc; j++) { sem_wait(&empty); sem_wait(&mutex); put(-1); sem_post(&mutex); sem_post(&full); }
    for (int j = 0; j < nc; j++) pthread_join(th[np + j], NULL);

    long psum = 0, pcnt = 0, csum = 0, ccnt = 0;
    for (int i = 0; i < np; i++) { psum += args[i].sum; pcnt += args[i].cnt; }
    for (int j = 0; j < nc; j++) {
        printf("   消费者 C%d 取走 %ld 个\n", j, args[np + j].cnt);
        csum += args[np + j].sum; ccnt += args[np + j].cnt;
    }
    printf("生产 %ld 个 (校验和 %ld)，消费 %ld 个 (校验和 %ld) → %s\n",
           pcnt, psum, ccnt, csum, (pcnt == ccnt && psum == csum) ? "一致 ✓" : "不一致 ✗");
    return 0;
}
