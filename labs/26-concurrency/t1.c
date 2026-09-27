// t1.c：原书 Figure 26.6 —— 两个线程各把共享计数器加 loops 次（默认 1e7）
// 用法：./t1 [loops]
#include <stdio.h>
#include <stdlib.h>
#include "common_threads.h"

static volatile int counter = 0;
static int loops = 10000000;

void *mythread(void *arg) {
    printf("%s: begin\n", (char *) arg);
    for (int i = 0; i < loops; i++) {
        counter = counter + 1;      // 临界区：读-改-写三条指令，不是原子的
    }
    printf("%s: done\n", (char *) arg);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc > 1) loops = atoi(argv[1]);
    pthread_t p1, p2;
    printf("main: begin (counter = %d)\n", counter);
    Pthread_create(&p1, NULL, mythread, "A");
    Pthread_create(&p2, NULL, mythread, "B");
    Pthread_join(p1, NULL);
    Pthread_join(p2, NULL);
    printf("main: done with both (counter = %d, expected %d)\n", counter, 2 * loops);
    return 0;
}
