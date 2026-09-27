// mmap_touch.c —— 按需分页 + 按需清零 + 大页
// 1) mmap 256 MB 匿名内存：虚拟空间立即有了，物理内存（RSS）几乎不变
// 2) 逐步"写"触碰 25%、50%、100% 的页：RSS 随之增长，每页一次 minor fault（按需分配并清零）
// 3) 另一块区域只"读"不写：每页也有一次页错误，但映射的是共享的零页，RSS 不增长
// 4) 用 madvise(MADV_HUGEPAGE) 请求 2MB 透明大页：页错误次数约少 512 倍
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#define LEN (256UL << 20)
static long pg;
static long rss_kb(void) { long size, res; FILE *f = fopen("/proc/self/statm", "r"); if (fscanf(f, "%ld %ld", &size, &res) != 2) res = -1; fclose(f); return res * pg / 1024; }
static long minflt(void) { struct rusage r; getrusage(RUSAGE_SELF, &r); return r.ru_minflt; }
static double now(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec * 1e3 + t.tv_usec / 1e3; }

static void region(const char *name, int huge) {
    char *p = mmap(NULL, LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) { perror("mmap"); exit(1); }
    if (huge && madvise(p, LEN, MADV_HUGEPAGE) != 0) perror("madvise(MADV_HUGEPAGE)");
    if (!huge) madvise(p, LEN, MADV_NOHUGEPAGE);
    printf("== %s ==\n", name);
    printf("  after mmap          : RSS %7ld kB\n", rss_kb());
    long f = minflt(); double t = now();
    size_t done = 0;
    for (int q = 1; q <= 4; q *= 2) {
        size_t upto = LEN / 4 * q;
        for (; done < upto; done += pg) p[done] = 1;      // 写触碰
        printf("  wrote %3d%% of pages : RSS %7ld kB   minor faults so far %6ld   (%.1f ms)\n",
               q * 25, rss_kb(), minflt() - f, now() - t);
    }
    munmap(p, LEN);
}

int main(void) {
    pg = sysconf(_SC_PAGESIZE);
    region("4KB pages (demand paging + demand zeroing)", 0);

    char *r = mmap(NULL, LEN, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    madvise(r, LEN, MADV_NOHUGEPAGE);
    long f = minflt(), before = rss_kb(); volatile long sum = 0;
    for (size_t i = 0; i < LEN; i += pg) sum += r[i];    // 只读
    printf("== read-only touch of 256 MB ==\n  minor faults %ld, RSS grew %ld kB, sum of bytes = %ld (all zero)\n",
           minflt() - f, rss_kb() - before, (long)sum);
    munmap(r, LEN);

    region("2MB transparent huge pages (MADV_HUGEPAGE)", 1);
    return 0;
}
