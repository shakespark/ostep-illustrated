// 第 10 章实验：缓存亲和性（cache affinity）
//
// 一个线程反复"走一遍"自己的工作集：按随机顺序访问每条 64 字节缓存行（指针追逐，
// 下一条的位置存在上一条里，硬件预取猜不到），并顺手写一下，让这些行在缓存里变"脏"。
// 比较两种放置方式下，走一遍的耗时（折算成每条缓存行多少纳秒）：
//   pin      一直待在同一个 CPU 上：数据留在本核的 L1/L2 里，之后每遍都很快
//   migrate  每遍之后换到另一个 CPU：新 CPU 的缓存是冷的，而最新的数据还躺在旧 CPU 的缓存里，
//            每条行都要经缓存一致性协议从旧 CPU 搬过来（或从 L3 / 内存重新读）
// 工作集超过最后一级缓存（L3）之后，无论放在哪里都得从内存读，亲和性就不再重要。
//
// 用法：./affinity                    扫描多种工作集大小
//       ./affinity <KB> [遍数]         只测一种大小
//       ./affinity <KB> <遍数> <cpuA> <cpuB>   指定在哪两个 CPU 之间迁移
#define _GNU_SOURCE
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#define LINE 64
typedef struct line { struct line *next; long val; char pad[LINE - sizeof(void *) - sizeof(long)]; } line_t;

static double now_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e9 + ts.tv_nsec;
}

static void move_to(int cpu) {
    cpu_set_t set;
    CPU_ZERO(&set);
    CPU_SET(cpu, &set);
    if (sched_setaffinity(0, sizeof set, &set) != 0) { perror("sched_setaffinity"); exit(1); }
    sched_yield();  // 让迁移立即生效
}

// 把 n 条缓存行串成一个随机顺序的环
static line_t *make_ring(size_t n) {
    line_t *a = aligned_alloc(4096, n * sizeof(line_t));
    size_t *perm = malloc(n * sizeof(size_t));
    for (size_t i = 0; i < n; i++) perm[i] = i;
    srand(42);
    for (size_t i = n - 1; i > 0; i--) { size_t j = ((size_t)rand() * RAND_MAX + rand()) % (i + 1); size_t t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
    for (size_t i = 0; i < n; i++) { a[perm[i]].next = &a[perm[(i + 1) % n]]; a[perm[i]].val = 0; }
    free(perm);
    return a;
}

// 走一遍：沿环访问每条缓存行一次，读下一跳并写本行
static double one_pass(line_t *start, size_t n) {
    double t0 = now_ns();
    line_t *p = start;
    for (size_t i = 0; i < n; i++) { p->val++; p = p->next; }
    double t1 = now_ns();
    if (p != start) fprintf(stderr, "环断了\n");
    return t1 - t0;
}

static int cmp(const void *a, const void *b) {
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

// migrate = 0：一直待在 a；migrate = 1：每遍之后在 a、b 之间来回换。返回每条缓存行的中位纳秒数
static double run(int migrate, size_t kb, int passes, int a, int b) {
    size_t n = kb * 1024 / LINE;
    line_t *ring = make_ring(n);
    double *t = malloc(sizeof(double) * passes);
    int cpu = a;
    move_to(cpu);
    one_pass(ring, n);  // 预热：数据先进入 a 的缓存
    for (int p = 0; p < passes; p++) {
        if (migrate) { cpu = (cpu == a) ? b : a; move_to(cpu); }  // 换 CPU 的开销不计入
        t[p] = one_pass(ring, n);
    }
    qsort(t, passes, sizeof(double), cmp);
    double med = t[passes / 2] / n;
    free(t); free(ring);
    return med;
}

int main(int argc, char *argv[]) {
    int ncpu = sysconf(_SC_NPROCESSORS_ONLN);
    int a = 0, b = ncpu > 2 ? 2 : 1;  // 默认 CPU 0 和 CPU 2（在常见的 Linux 编号里通常属于不同物理核）
    if (ncpu < 2) { fprintf(stderr, "需要至少 2 个 CPU\n"); return 1; }

    size_t sizes[] = {16, 64, 256, 1024, 4096, 16384, 65536};
    int nsizes = sizeof sizes / sizeof sizes[0], passes = 0;
    if (argc > 1) { sizes[0] = atoi(argv[1]); nsizes = 1; }
    if (argc > 2) passes = atoi(argv[2]);
    if (argc > 4) { a = atoi(argv[3]); b = atoi(argv[4]); }

    printf("在线 CPU %d 个；pin = 一直在 CPU %d；migrate = 每遍之后在 CPU %d 和 CPU %d 之间换\n", ncpu, a, a, b);
    printf("每种配置取各遍耗时的中位数，单位：纳秒 / 缓存行（随机顺序访问）\n\n");
    printf("%10s %10s %10s %14s\n", "工作集", "pin", "migrate", "migrate/pin");
    for (int i = 0; i < nsizes; i++) {
        size_t kb = sizes[i];
        int ps = passes ? passes : (kb <= 1024 ? 401 : kb <= 16384 ? 41 : 9);
        double p0 = run(0, kb, ps, a, b);
        double p1 = run(1, kb, ps, a, b);
        char label[32];
        if (kb >= 1024) snprintf(label, sizeof label, "%zu MB", kb / 1024);
        else snprintf(label, sizeof label, "%zu KB", kb);
        printf("%10s %10.2f %10.2f %13.1fx\n", label, p0, p1, p1 / p0);
    }
    return 0;
}
