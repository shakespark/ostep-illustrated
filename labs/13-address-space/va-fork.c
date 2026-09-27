// va-fork.c —— 同一个虚拟地址，在两个进程里是两块不同的物理内存。
// fork() 之后父子进程的地址空间"长得一模一样"：&x 打印出来相同，
// 但子进程改写 x 之后，父进程看到的 x 仍然是旧值。
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int x = 100;               // 全局变量（数据段）

int main(void) {
    int *h = malloc(sizeof(int));   // 堆上的一个整数
    *h = 1;
    pid_t pid = fork();
    if (pid < 0) { perror("fork"); exit(1); }
    if (pid == 0) {                  // 子进程：改写两个变量
        x = 200; *h = 2;
        printf("[child  pid=%d] &x=%p x=%d | h=%p *h=%d\n", getpid(), (void *) &x, x, (void *) h, *h);
        exit(0);
    }
    wait(NULL);                      // 等子进程先打印
    printf("[parent pid=%d] &x=%p x=%d | h=%p *h=%d\n", getpid(), (void *) &x, x, (void *) h, *h);
    free(h);
    return 0;
}
