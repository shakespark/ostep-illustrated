// barrier.c —— 用两个信号量 + 计数器实现屏障（Homework 第 3 题的一种解法）
//   ./barrier [线程数]
// 保证：所有线程都执行完 P1 之后，任何线程才会执行 P2。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

int n = 5, count = 0;
sem_t mutex;       // 保护 count，初值 1
sem_t turnstile;   // 旋转门，初值 0：最后一个到达者把门打开
int p1_done = 0, violated = 0;

void barrier(void) {
    sem_wait(&mutex);
    count++;
    if (count == n) sem_post(&turnstile);   // 最后一个到达：开门
    sem_post(&mutex);
    sem_wait(&turnstile);                   // 在门口等
    sem_post(&turnstile);                   // 自己过去后，再把门留给下一个人
}

void *worker(void *arg) {
    long id = (long)arg;
    usleep((rand() % 5) * 100000 + id * 1000);   // 每个线程到达时间不同
    printf("线程 %ld: P1\n", id);
    __sync_fetch_and_add(&p1_done, 1);
    barrier();
    if (p1_done != n) violated = 1;              // 过了屏障，所有人的 P1 必须都完成了
    printf("线程 %ld:     P2 (此时已完成 P1 的线程数 = %d)\n", id, p1_done);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1) n = atoi(argv[1]);
    srand(42);
    sem_init(&mutex, 0, 1);
    sem_init(&turnstile, 0, 0);
    pthread_t th[64];
    if (n < 1 || n > 64) return 1;
    for (long i = 0; i < n; i++) pthread_create(&th[i], NULL, worker, (void *)i);
    for (int i = 0; i < n; i++) pthread_join(th[i], NULL);
    printf("%s\n", violated ? "✗ 有线程在别人完成 P1 之前就执行了 P2" : "✓ 所有 P2 都发生在全部 P1 之后");
    return 0;
}
