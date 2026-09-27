// dining.c —— 哲学家就餐问题（原书 31.6 节）
//   ./dining broken [轮数]   每人先拿左叉再拿右叉（Figure 31.15）→ 很快死锁
//   ./dining fixed  [轮数]   哲学家 4 先拿右叉再拿左叉（Figure 31.16）→ 不会死锁
// 为了让死锁"几乎必然"出现，拿到第一把叉子后故意 usleep 一小会儿，放大竞争窗口。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <time.h>

#define N 5
sem_t forks[N];
int fixed = 0, rounds = 1000;
pthread_mutex_t st_lock = PTHREAD_MUTEX_INITIALIZER;   // 只保护下面的"状态报告"数组
int holding[N][2];      // 哲学家 p 手里拿着的叉子（-1 表示没有）
int waiting_for[N];     // 正在等哪把叉子（-1 表示没在等）
long meals[N];
volatile long total = 0;

int left(int p)  { return p; }
int right(int p) { return (p + 1) % N; }

void set_state(int p, int slot, int fork, int wait) {
    pthread_mutex_lock(&st_lock);
    if (slot >= 0) holding[p][slot] = fork;
    waiting_for[p] = wait;
    pthread_mutex_unlock(&st_lock);
}

void get_forks(int p) {
    int first = left(p), second = right(p);
    if (fixed && p == 4) { first = right(p); second = left(p); }   // 打破循环等待
    set_state(p, -1, 0, first);
    sem_wait(&forks[first]);
    set_state(p, 0, first, -1);
    usleep(1000);                         // 拿着一把叉子"发会儿呆"：放大死锁窗口
    set_state(p, -1, 0, second);
    sem_wait(&forks[second]);
    set_state(p, 1, second, -1);
}

void put_forks(int p) {
    sem_post(&forks[left(p)]);
    sem_post(&forks[right(p)]);
    set_state(p, 0, -1, -1);
    set_state(p, 1, -1, -1);
}

void *philosopher(void *arg) {
    int p = (int)(long)arg;
    unsigned seed = p * 7919u + 1;
    for (int i = 0; i < rounds; i++) {
        usleep(rand_r(&seed) % 200);      // think()
        get_forks(p);
        meals[p]++; __sync_fetch_and_add(&total, 1);   // eat()
        usleep(rand_r(&seed) % 200);
        put_forks(p);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1) fixed = strcmp(argv[1], "fixed") == 0;
    if (argc > 2) rounds = atoi(argv[2]);
    printf("%s：5 位哲学家，每人吃 %d 次\n", fixed ? "修复版（P4 先右后左）" : "错误版（全部先左后右）", rounds);
    for (int i = 0; i < N; i++) { sem_init(&forks[i], 0, 1); holding[i][0] = holding[i][1] = waiting_for[i] = -1; }
    struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
    pthread_t th[N];
    for (long i = 0; i < N; i++) pthread_create(&th[i], NULL, philosopher, (void *)i);

    long last = -1; int stall = 0;
    while (total < (long)N * rounds) {
        usleep(100000);
        if (total == last) {
            if (++stall >= 20) {           // 2 秒没人吃上饭
                printf("!! 2 秒内没有人吃上饭：检测到死锁（已吃 %ld 顿）\n", total);
                for (int p = 0; p < N; p++)
                    printf("   P%d 拿着 f%d，等待 f%d\n", p,
                           holding[p][0] >= 0 ? holding[p][0] : holding[p][1], waiting_for[p]);
                printf("   等待链：P0→f1(P1持有)→f2(P2持有)→…→f0(P0持有)，形成环\n");
                return 2;                  // 直接退出，不然会永远卡住
            }
        } else { stall = 0; last = total; }
    }
    for (int i = 0; i < N; i++) pthread_join(th[i], NULL);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    printf("全部吃完，用时 %.2f 秒；每人吃的次数：", (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9);
    for (int p = 0; p < N; p++) printf(" P%d=%ld", p, meals[p]);
    printf("\n");
    return 0;
}
