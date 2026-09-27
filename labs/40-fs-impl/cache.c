// cache.c —— 感受写缓冲与读缓存：fsync 的代价、同一文件读两次、O_DIRECT 绕过页缓存
// 用法：./cache [MB]   默认 64MB，测试文件建在当前目录，结束时删除
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static char buf[1 << 20] __attribute__((aligned(4096)));   // O_DIRECT 要求缓冲区按块对齐

// 写 mb MB：分别计时 write() 循环与随后的 fsync()
static void write_then_fsync(const char *p, int mb, double *tw, double *tf) {
    unlink(p);
    int fd = open(p, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) { perror("open"); exit(1); }
    double t0 = now();
    for (int i = 0; i < mb; i++) if (write(fd, buf, sizeof buf) != sizeof buf) { perror("write"); exit(1); }
    double t1 = now();
    fsync(fd);
    double t2 = now();
    close(fd);
    *tw = t1 - t0; *tf = t2 - t1;
}
// n 次 4KB 追加写，每次之后是否 fsync（模拟数据库每次提交都要落盘）
static double small_writes(const char *p, int n, int sync_each) {
    unlink(p);
    int fd = open(p, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) { perror("open"); exit(1); }
    double t0 = now();
    for (int i = 0; i < n; i++) {
        if (write(fd, buf, 4096) != 4096) { perror("write"); exit(1); }
        if (sync_each) fsync(fd);
    }
    double t = now() - t0;
    close(fd);
    return t;
}
static double read_file(const char *p, int flags) {
    double t0 = now();
    int fd = open(p, O_RDONLY | flags);
    if (fd < 0) { perror("open"); exit(1); }
    while (read(fd, buf, sizeof buf) > 0) ;
    close(fd);
    return now() - t0;
}

int main(int argc, char *argv[]) {
    int mb = argc > 1 ? atoi(argv[1]) : 64;
    const char *p = "cache-test.dat";
    memset(buf, 'x', sizeof buf);

    printf("== 写缓冲：write() 返回 ≠ 数据已落盘 ==\n");
    double s0 = small_writes(p, 200, 0), s1 = small_writes(p, 200, 1);
    printf("200 次 4KB 追加写，不 fsync   : %8.2f ms（平均每次 %6.1f µs）\n", s0 * 1e3, s0 * 1e6 / 200);
    printf("200 次 4KB 追加写，每次 fsync : %8.2f ms（平均每次 %6.1f µs）\n", s1 * 1e3, s1 * 1e6 / 200);
    double tw, tf;
    write_then_fsync(p, mb, &tw, &tf);
    printf("写 %d MB：write() 循环用时 %8.1f ms  ← 数据只是进了页缓存（脏页）\n", mb, tw * 1e3);
    printf("          随后 fsync() 用时 %8.1f ms  ← 这时才等设备把脏页真正写下去\n", tf * 1e3);
    printf("\n== 读缓存：第二次读几乎不碰磁盘 ==\n");
    int fd = open(p, O_RDONLY);
    fdatasync(fd);
    posix_fadvise(fd, 0, 0, POSIX_FADV_DONTNEED);   // 请内核把这个文件的干净页踢出页缓存
    close(fd);
    double r1 = read_file(p, 0);
    double r2 = read_file(p, 0);
    double r3 = read_file(p, O_DIRECT);
    printf("第 1 次读（已尽量清缓存）  : %8.1f ms  (%5.0f MB/s)\n", r1 * 1e3, mb / r1);
    printf("第 2 次读（命中页缓存）    : %8.1f ms  (%5.0f MB/s)\n", r2 * 1e3, mb / r2);
    printf("O_DIRECT 读（绕过页缓存）  : %8.1f ms  (%5.0f MB/s)\n", r3 * 1e3, mb / r3);
    unlink(p);
    return 0;
}
