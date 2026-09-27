// var.c —— 作业题 1：fork 之后，父子进程的变量是各自独立的副本
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int x = 100;
    int *heap = malloc(sizeof(int));
    *heap = 100;
    printf("fork 前: x=%d  *heap=%d  &x=%p  heap=%p\n", x, *heap, (void *) &x, (void *) heap);
    fflush(stdout);
    int rc = fork();
    if (rc == 0) {
        x += 1; *heap += 1;
        printf("child : x=%d  *heap=%d  &x=%p  heap=%p\n", x, *heap, (void *) &x, (void *) heap);
        exit(0);
    }
    wait(NULL);                       // 等孩子改完再看自己的
    x -= 1; *heap -= 1;
    printf("parent: x=%d   *heap=%d   &x=%p  heap=%p\n", x, *heap, (void *) &x, (void *) heap);
    return 0;
}
