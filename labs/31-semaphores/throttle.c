// throttle.c —— 用信号量做线程节流（原书 31.7 节）
//   ./throttle [线程数] [K]   K = 同时允许进入"内存密集区"的线程数；K=0 表示不节流
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

int nthreads = 20, K = 3;
sem_t gate;
int inside = 0, max_inside = 0;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *worker(void *arg) {
    long id = (long)arg;
    usleep((id % 4) * 1000);                     // 一些"普通计算"
    if (K > 0) sem_wait(&gate);                  // 进入内存密集区前先过闸门
    pthread_mutex_lock(&m);
    inside++; if (inside > max_inside) max_inside = inside;
    pthread_mutex_unlock(&m);
    char *big = malloc(8 << 20);                 // 假装需要一大块内存（8MB）
    for (int i = 0; i < (8 << 20); i += 4096) big[i] = 1;
    usleep(20000);
    free(big);
    pthread_mutex_lock(&m); inside--; pthread_mutex_unlock(&m);
    if (K > 0) sem_post(&gate);                  // 离开时把名额还回去
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1) nthreads = atoi(argv[1]);
    if (argc > 2) K = atoi(argv[2]);
    if (nthreads < 1 || nthreads > 256) return 1;
    if (K > 0) sem_init(&gate, 0, K);            // 初值 = 允许同时进入的线程数
    pthread_t th[256];
    for (long i = 0; i < nthreads; i++) pthread_create(&th[i], NULL, worker, (void *)i);
    for (int i = 0; i < nthreads; i++) pthread_join(th[i], NULL);
    printf("%d 个线程，K=%d%s：内存密集区里同时最多有 %d 个线程（峰值约 %d MB）\n",
           nthreads, K, K > 0 ? "" : "（不节流）", max_inside, max_inside * 8);
    return 0;
}
