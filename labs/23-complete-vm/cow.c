// cow.c —— 观察 fork() 的写时复制（copy-on-write）
// 父进程先分配并写满 64 MB；fork 后子进程：
//   1) 读 /proc/self/smaps 看这块区域是"共享"的（Shared_Dirty）
//   2) 逐页写一个字节：每页第一次写都触发 COW 页错误（复制一页），计时并统计 minor fault
//   3) 再写一遍：页已私有，不再有页错误，快得多
//   4) 再看 smaps：变成了 Private_Dirty
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>

#define MB 64
static double now(void) { struct timeval t; gettimeofday(&t, NULL); return t.tv_sec * 1e3 + t.tv_usec / 1e3; }
static long minflt(void) { struct rusage r; getrusage(RUSAGE_SELF, &r); return r.ru_minflt; }

// 从 /proc/self/smaps 中找到包含 addr 的那个映射，打印 Rss / Shared_Dirty / Private_Dirty
static void smaps(const char *who, void *addr) {
    FILE *f = fopen("/proc/self/smaps", "r");
    char line[512]; int in = 0; unsigned long lo, hi, target = (unsigned long)addr;
    long rss = -1, sd = -1, pd = -1;
    while (fgets(line, sizeof line, f)) {
        if (sscanf(line, "%lx-%lx ", &lo, &hi) == 2 && strchr(line, '-') < strchr(line, ' ')) { in = target >= lo && target < hi; continue; }
        if (!in) continue;
        sscanf(line, "Rss: %ld kB", &rss);
        sscanf(line, "Shared_Dirty: %ld kB", &sd);
        sscanf(line, "Private_Dirty: %ld kB", &pd);
    }
    fclose(f);
    printf("  [%s] smaps: Rss %6ld kB   Shared_Dirty %6ld kB   Private_Dirty %6ld kB\n", who, rss, sd, pd);
}

int main(void) {
    size_t len = (size_t)MB << 20, pg = sysconf(_SC_PAGESIZE);
    char *buf = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    madvise(buf, len, MADV_NOHUGEPAGE);          // 用 4KB 页，便于数页错误
    memset(buf, 1, len);                          // 父进程先把 64MB 全部写一遍
    printf("parent: %d MB touched (%zu pages)\n", MB, len / pg);
    smaps("parent before fork", buf);

    fflush(stdout);                               // fork 前清空缓冲区，避免输出被复制两份
    double t0 = now();
    pid_t pid = fork();
    if (pid == 0) {
        printf("child : fork() returned after %.2f ms (only page tables copied, pages shared read-only)\n", now() - t0);
        smaps("child  after fork ", buf);
        long f0 = minflt(); double a = now();
        for (size_t i = 0; i < len; i += pg) buf[i] = 2;   // 第一次写：每页一次 COW 页错误
        double b = now(); long f1 = minflt();
        for (size_t i = 0; i < len; i += pg) buf[i] = 3;   // 第二次写：页已私有
        double c = now(); long f2 = minflt();
        printf("child : 1st write pass %7.2f ms, minor faults %ld   <- copy-on-write\n", b - a, f1 - f0);
        printf("child : 2nd write pass %7.2f ms, minor faults %ld\n", c - b, f2 - f1);
        smaps("child  after write", buf);
        fflush(stdout);
        _exit(0);
    }
    waitpid(pid, NULL, 0);
    printf("parent: buf[0] is still %d (child's writes went to its private copies)\n", buf[0]);
    return 0;
}
