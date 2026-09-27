// seqrand.c —— 顺序读 vs 随机读：用 O_DIRECT 绕过页缓存，直接测"设备"的表现
// 用法：./seqrand [文件大小MB，默认 128，最大 256] [随机读次数，默认 4000]
// 程序会在当前目录创建 seqrand-test.dat，测完自动删除。
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }

int main(int argc, char *argv[]) {
    long mb = argc > 1 ? atol(argv[1]) : 128;
    long nrand = argc > 2 ? atol(argv[2]) : 4000;
    if (mb < 8) mb = 8;
    if (mb > 256) mb = 256;
    const char *path = "seqrand-test.dat";
    const size_t BIG = 1 << 20, SMALL = 4096;
    char *buf;
    if (posix_memalign((void **)&buf, 4096, BIG) != 0) { perror("posix_memalign"); return 1; }  // O_DIRECT 要求对齐
    memset(buf, 'x', BIG);

    // 1. 准备测试文件（普通写 + fsync）
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) { perror("open"); return 1; }
    for (long i = 0; i < mb; i++) if (write(fd, buf, BIG) != (ssize_t)BIG) { perror("write"); return 1; }
    fsync(fd); close(fd);

    fd = open(path, O_RDONLY | O_DIRECT);
    if (fd < 0) { perror("open O_DIRECT（文件系统可能不支持）"); unlink(path); return 1; }
    long nblk = mb * (BIG / SMALL);
    printf("测试文件 %ld MB = %ld 个 4KB 块（O_DIRECT，绕过页缓存）\n\n", mb, nblk);

    // 2. 顺序读：1MB 一次
    double t0 = now();
    for (long i = 0; i < mb; i++) if (pread(fd, buf, BIG, i * BIG) != (ssize_t)BIG) { perror("pread"); return 1; }
    double tseq = now() - t0;
    printf("顺序读  1MB × %-5ld : %8.1f ms  %8.1f MB/s\n", mb, tseq * 1e3, mb / tseq);

    // 3. 顺序读：4KB 一次（请求小，但位置连续）
    long nseq4 = nrand < nblk ? nrand : nblk;
    t0 = now();
    for (long i = 0; i < nseq4; i++) if (pread(fd, buf, SMALL, i * SMALL) != (ssize_t)SMALL) { perror("pread"); return 1; }
    double tseq4 = now() - t0;
    printf("顺序读  4KB × %-5ld : %8.1f ms  %8.1f MB/s  平均每次 %6.1f us\n", nseq4, tseq4 * 1e3, nseq4 * 4.0 / 1024 / tseq4, tseq4 / nseq4 * 1e6);

    // 4. 随机读：4KB 一次，位置随机
    srand(42);
    t0 = now();
    for (long i = 0; i < nrand; i++) {
        long b = ((long)rand() * 7919L + rand()) % nblk;
        if (pread(fd, buf, SMALL, b * SMALL) != (ssize_t)SMALL) { perror("pread"); return 1; }
    }
    double trand = now() - t0;
    printf("随机读  4KB × %-5ld : %8.1f ms  %8.1f MB/s  平均每次 %6.1f us\n", nrand, trand * 1e3, nrand * 4.0 / 1024 / trand, trand / nrand * 1e6);

    printf("\n顺序(1MB) / 随机(4KB) 带宽比 ≈ %.0f 倍\n", (mb / tseq) / (nrand * 4.0 / 1024 / trand));
    close(fd);
    unlink(path);
    free(buf);
    return 0;
}
