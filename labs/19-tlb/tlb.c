// tlb.c —— 用"每页访问一次"的循环测量 TLB 的大小和 miss 代价（OSTEP 第 19 章作业）
//
// 用法：./tlb <页数> <趟数> [选项]
//   -c CPU   把线程绑定到指定 CPU（默认 0）；-c -1 表示不绑定
//   -s       按顺序访问页（默认把页的访问顺序随机打乱，避免硬件预取"帮忙"）
//   -n       不做缓存行错位（默认每页访问的位置错开一个缓存行，见 README）
//   -k K     重复测量 K 次，报告最小值和中位数（默认 5）
//   -H       允许透明大页（默认用 madvise 禁止，保证是 4KB 小页）
//   -p       指针追逐模式：每次访问读出"下一页的位置"，访问之间有数据依赖，测的是延迟
//   -q       只输出一行：页数 最小值 中位数（给 run.sh 用）
#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static int cmp_double(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "用法: %s <页数> <趟数> [-c cpu] [-s] [-n] [-k 次数] [-H] [-q]\n", argv[0]);
        return 1;
    }
    long npages = atol(argv[1]);
    long trials = atol(argv[2]);
    int cpu = 0, sequential = 0, no_skew = 0, reps = 5, allow_huge = 0, quiet = 0, chase = 0;
    for (int i = 3; i < argc; i++) {
        if (!strcmp(argv[i], "-c") && i + 1 < argc) cpu = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-s")) sequential = 1;
        else if (!strcmp(argv[i], "-n")) no_skew = 1;
        else if (!strcmp(argv[i], "-k") && i + 1 < argc) reps = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-H")) allow_huge = 1;
        else if (!strcmp(argv[i], "-q")) quiet = 1;
        else if (!strcmp(argv[i], "-p")) chase = 1;
    }
    if (npages < 1 || trials < 1 || reps < 1 || reps > 99) { fprintf(stderr, "参数不合法\n"); return 1; }

    // 1. 绑定 CPU：每个核有自己的 TLB，线程在核之间迁移会让测量失真（作业第 6 题）
    if (cpu >= 0) {
        cpu_set_t set;
        CPU_ZERO(&set);
        CPU_SET(cpu, &set);
        if (sched_setaffinity(0, sizeof(set), &set) != 0) perror("sched_setaffinity");
    }

    // 2. 分配数组：用 mmap 按页对齐；禁止透明大页，否则 512 个 4KB "页"可能共用一个 2MB TLB 项
    long pagesize = sysconf(_SC_PAGESIZE);
    size_t bytes = (size_t)npages * pagesize;
    char *mem = mmap(NULL, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) { perror("mmap"); return 1; }
    if (!allow_huge) madvise(mem, bytes, MADV_NOHUGEPAGE);
    // volatile：告诉编译器每次 a[i] += 1 都必须真的读写内存，不能被优化掉（作业第 5 题）
    volatile int *a = (volatile int *)mem;

    // 3. 预先写一遍每一页：把"按需清零 / 缺页"的开销挪到计时之外（作业第 7 题）
    for (long p = 0; p < npages; p++) mem[p * pagesize] = 1;

    // 4. 计算每一页上要访问的下标。jump = 一页里有多少个 int
    long jump = pagesize / sizeof(int);
    long lines = pagesize / 64;                 // 一页 64 个缓存行
    long *idx = malloc(sizeof(long) * npages);
    for (long p = 0; p < npages; p++) {
        long skew = no_skew ? 0 : (p % lines) * (64 / sizeof(int));
        idx[p] = p * jump + skew;
    }
    if (!sequential) {                          // 打乱访问顺序（固定种子，可复现）
        srand(12345);
        for (long i = npages - 1; i > 0; i--) {
            long j = rand() % (i + 1);
            long t = idx[i]; idx[i] = idx[j]; idx[j] = t;
        }
    }

    // 指针追逐模式：在第 idx[p] 个 int 里写入"下一个要访问的下标"，串成一个环
    if (chase)
        for (long p = 0; p < npages; p++) a[idx[p]] = (int)idx[(p + 1) % npages];

    // 5. 计时。先热身约 0.2 秒：让 TLB/缓存进入稳定状态，也让 CPU 从省电状态升到正常频率
    //    （WSL2/笔记本上 CPU 空闲后频率会降低，不热身的话前几个测量点会偏慢好几倍）
    long cur = idx[0];
    double warm_end = now_ns() + 2e8;
    while (now_ns() < warm_end) {
        if (chase) { for (long p = 0; p < npages; p++) cur = a[cur]; }
        else { for (long p = 0; p < npages; p++) a[idx[p]] += 1; }
    }
    double res[100];
    for (int r = 0; r < reps; r++) {
        double t0 = now_ns();
        if (chase) {
            // 每次访问的地址取决于上一次读出的值：CPU 没法提前发出下一次访问
            for (long t = 0; t < trials; t++)
                for (long p = 0; p < npages; p++)
                    cur = a[cur];
        } else {
            // 原书作业里的循环：每页更新一个 int
            for (long t = 0; t < trials; t++)
                for (long p = 0; p < npages; p++)
                    a[idx[p]] += 1;
        }
        double t1 = now_ns();
        res[r] = (t1 - t0) / ((double)trials * npages);
    }
    if (cur == -1) fprintf(stderr, "\n");
    qsort(res, reps, sizeof(double), cmp_double);
    double best = res[0], median = res[reps / 2];

    if (quiet) {
        printf("%ld %.2f %.2f\n", npages, best, median);
    } else {
        struct timespec res_ts;
        clock_getres(CLOCK_MONOTONIC, &res_ts);
        printf("页大小 %ld 字节，页数 %ld，趟数 %ld，重复 %d 次，CPU %d，%s，%s访问，%s\n",
               pagesize, npages, trials, reps, cpu, chase ? "指针追逐(-p)" : "a[i] += 1",
               sequential ? "顺序" : "随机顺序",
               no_skew ? "不错开缓存行" : "每页错开一个缓存行");
        printf("clock_gettime 分辨率：%ld ns\n", res_ts.tv_nsec);
        printf("每次访问平均耗时：最小 %.2f ns，中位数 %.2f ns\n", best, median);
    }
    // 用一下结果，防止整个循环被当成"没人用的计算"删掉
    long sum = 0;
    if (!chase) for (long p = 0; p < npages; p++) sum += a[idx[p]];
    if (sum == 42) fprintf(stderr, "\n");
    free(idx);
    munmap(mem, bytes);
    return 0;
}
