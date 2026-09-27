// seqbuf.c —— 用真实磁盘体会 LFS 的出发点：
//   1) 随机原地小写（传统文件系统更新 inode/位图/目录的样子）
//   2) 顺序但每块都立刻落盘（“只顺序、不缓冲”，原书 43.2 说这还不够）
//   3) 先在内存里攒成一个“段”，再一次性顺序写下去（LFS 的写缓冲）
// 每次“落盘”都用 fdatasync()，保证数据真的到了设备，而不是停在页缓存里。
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define BLK 4096
#define FILE_MB 64

static double now(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}
static void die(const char *m) { perror(m); exit(1); }

static void report(const char *mode, long seg, long total, int syncs, double t) {
    printf("%-16s %8ld KB %8ld KB %7d %9.3f %9.2f %9.3f\n", mode, seg / 1024, total / 1024, syncs, t,
           total / 1048576.0 / t, t * 1000 / syncs);
}

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "lfs_test.dat";
    int nsmall = argc > 2 ? atoi(argv[2]) : 500;   // 模式 1、2 的 4KB 写次数
    long seqtotal = 64L << 20;                        // 模式 3 每种段大小共写 64MB
    int fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) die("open");
    char *buf;
    if (posix_memalign((void **)&buf, BLK, 8L << 20)) die("posix_memalign");
    memset(buf, 'L', 8L << 20);
    // 预先把文件写满并落盘，避免后面测到“分配新块”的开销
    for (long off = 0; off < (long)FILE_MB << 20; off += 8L << 20)
        if (pwrite(fd, buf, 8L << 20, off) != 8L << 20) die("pwrite");
    if (fsync(fd)) die("fsync");

    printf("文件 %s，%d MB；每次落盘都调用 fdatasync()\n", path, FILE_MB);
    printf("%-16s %11s %11s %7s %9s %9s %9s\n", "mode", "write", "total", "syncs", "time(s)", "MB/s", "ms/sync");

    srand(42);
    long nblocks = ((long)FILE_MB << 20) / BLK;
    double t0 = now();
    for (int i = 0; i < nsmall; i++) {
        long off = (rand() % nblocks) * BLK;
        if (pwrite(fd, buf, BLK, off) != BLK) die("pwrite");
        if (fdatasync(fd)) die("fdatasync");
    }
    report("1 rand-inplace", BLK, (long)nsmall * BLK, nsmall, now() - t0);

    t0 = now();
    for (int i = 0; i < nsmall; i++) {
        if (pwrite(fd, buf, BLK, (long)i * BLK) != BLK) die("pwrite");
        if (fdatasync(fd)) die("fdatasync");
    }
    report("2 seq-sync-each", BLK, (long)nsmall * BLK, nsmall, now() - t0);

    long segs[] = {16L << 10, 64L << 10, 256L << 10, 1L << 20, 4L << 20, 8L << 20};
    for (unsigned k = 0; k < sizeof segs / sizeof segs[0]; k++) {
        long seg = segs[k];
        int n = 0;
        t0 = now();
        for (long off = 0; off < seqtotal; off += seg) {
            if (pwrite(fd, buf, seg, off) != seg) die("pwrite");
            if (fdatasync(fd)) die("fdatasync");
            n++;
        }
        report("3 seg-buffered", seg, seqtotal, n, now() - t0);
    }
    close(fd);
    unlink(path);
    free(buf);
    return 0;
}
