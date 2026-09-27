// fork_join.c —— 用信号量实现 join（原书 Figure 31.6）
// 用法: ./fork_join [X]   X 为信号量初值，默认 0（正确）；试试 1 看看会发生什么
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

sem_t s;

void *child(void *arg) {
    (void)arg;
    sleep(1);                 // 故意让子线程晚一点，确认父线程真的在等
    printf("child\n");
    sem_post(&s);             // 信号：子线程完成了
    return NULL;
}

int main(int argc, char *argv[]) {
    int x = argc > 1 ? atoi(argv[1]) : 0;
    sem_init(&s, 0, x);       // 初值 X：应该是多少？
    printf("parent: begin (sem 初值 = %d)\n", x);
    pthread_t c;
    pthread_create(&c, NULL, child, NULL);
    sem_wait(&s);             // 在这里等子线程
    printf("parent: end\n");
    pthread_join(c, NULL);    // 仅为回收线程；X=1 时你会看到 child 在 end 之后才打印
    return 0;
}
