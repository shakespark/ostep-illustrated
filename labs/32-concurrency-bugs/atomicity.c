// atomicity.c —— 第 32 章 Figure 32.2 的原子性违反（MySQL proc_info）。
// 线程 1：if (proc_info) { 使用 proc_info }   —— "检查"和"使用"本应是原子的
// 线程 2：proc_info = NULL; 再恢复成非 NULL   —— 不断在两者之间插队
// 为了不真的段错误，"使用"时我们再读一次指针：若检查时非 NULL、使用时却变成 NULL，
// 就记一次"原子性违反"（真实程序在这里会解引用 NULL 而崩溃）。
// 用法: ./atomicity bug|fixed [iterations]
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pthread_barrier_t start;  // 让两个线程同时起跑，保证真的交错执行

static const char *volatile proc_info = "running";
static pthread_mutex_t proc_info_lock = PTHREAD_MUTEX_INITIALIZER;
static int fixed;
static long iters = 2000000;
static long violations, uses;

static void *thread1(void *arg) {
    (void)arg;
    pthread_barrier_wait(&start);
    for (long i = 0; i < iters; i++) {
        if (fixed) pthread_mutex_lock(&proc_info_lock);
        if (proc_info != NULL) {                 // 检查 (check)
            for (volatile int k = 0; k < 20; k++) // 模拟 check 与 use 之间的一小段代码
                ;
            const char *p = proc_info;           // 使用 (use)：相当于 fputs(thd->proc_info)
            if (p == NULL) violations++;         // 真实代码：fputs(NULL) → 崩溃
            else uses++;
        }
        if (fixed) pthread_mutex_unlock(&proc_info_lock);
    }
    return NULL;
}

static void *thread2(void *arg) {
    (void)arg;
    pthread_barrier_wait(&start);
    for (long i = 0; i < iters; i++) {
        if (fixed) pthread_mutex_lock(&proc_info_lock);
        proc_info = NULL;
        if (fixed) pthread_mutex_unlock(&proc_info_lock);
        if (fixed) pthread_mutex_lock(&proc_info_lock);
        proc_info = "running";
        if (fixed) pthread_mutex_unlock(&proc_info_lock);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || (strcmp(argv[1], "bug") && strcmp(argv[1], "fixed"))) {
        fprintf(stderr, "usage: %s bug|fixed [iterations]\n", argv[0]);
        return 1;
    }
    fixed = strcmp(argv[1], "fixed") == 0;
    if (argc > 2) iters = atol(argv[2]);
    pthread_barrier_init(&start, NULL, 2);
    pthread_t a, b;
    pthread_create(&a, NULL, thread1, NULL);
    pthread_create(&b, NULL, thread2, NULL);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    printf("[%s] 检查通过后成功使用 %ld 次；检查通过、使用时却已是 NULL（原子性违反）%ld 次\n",
           fixed ? "fixed" : "bug", uses, violations);
    return 0;
}
