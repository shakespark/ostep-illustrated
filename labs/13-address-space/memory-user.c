// memory-user.c —— 原书 13 章作业第 3 题：占用指定 MB 的内存并不停地访问它。
// 用法：./memory-user <MB> [秒数]      例：./memory-user 200 30
// 运行时在另一个终端用 free -m / pmap -x <pid> 观察内存变化。
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: %s <MB> [seconds]\n", argv[0]); return 1; }
    long mb = atol(argv[1]);
    int secs = argc > 2 ? atoi(argv[2]) : 10;
    size_t n = (size_t) mb * 1024 * 1024 / sizeof(int);
    int *arr = malloc(n * sizeof(int));
    if (!arr) { perror("malloc"); return 1; }
    printf("pid %d: using %ld MB at %p for %d s\n", getpid(), mb, (void *) arr, secs);
    fflush(stdout);
    time_t end = time(NULL) + secs;
    long passes = 0;
    while (time(NULL) < end) {                 // 反复"触摸"每个元素
        for (size_t i = 0; i < n; i++) arr[i] += 1;
        passes++;
    }
    printf("done: %ld passes over the array\n", passes);
    free(arr);
    return 0;
}
