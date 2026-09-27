// t1_lock.c：t1.c 的修正版 —— 用互斥锁保护临界区（预告第 27、28 章）
#include <stdio.h>
#include <stdlib.h>
#include "common_threads.h"

static volatile int counter = 0;
static int loops = 10000000;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

void *mythread(void *arg) {
    printf("%s: begin\n", (char *) arg);
    for (int i = 0; i < loops; i++) {
        Pthread_mutex_lock(&lock);
        counter = counter + 1;
        Pthread_mutex_unlock(&lock);
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
