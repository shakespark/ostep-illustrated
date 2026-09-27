// ext4_where.c —— 在真实的 ext4 上看"FFS 式放置"：
// 创建几个目录，每个目录里写几个小文件和一个大文件，fsync 后用 FIEMAP 查询
// 每个文件的数据落在哪个物理块、哪个块组（block group），再用 inode 号算出 inode 所在块组。
// 用法：./ext4_where [工作目录，默认 ./playground] [大文件 MB 数，默认 64]
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <linux/fiemap.h>

static long blksz, blocks_per_group, inodes_per_group;

static void die(const char *m) { perror(m); exit(1); }

// 从 /proc/fs/ext4/<dev>/mb_groups 数出块组个数（非 root 也可读）
static long count_groups(dev_t dev) {
    char link[256], path[512], name[80];
    snprintf(link, sizeof link, "/sys/dev/block/%u:%u", major(dev), minor(dev));
    ssize_t n = readlink(link, path, 511);
    if (n < 0) return -1;
    path[n] = 0;
    char *base = strrchr(path, '/');
    snprintf(name, sizeof name, "%.64s", base ? base + 1 : path);
    snprintf(path, sizeof path, "/proc/fs/ext4/%.64s/mb_groups", name);
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    char line[512];
    long groups = 0;
    while (fgets(line, sizeof line, f))
        if (line[0] == '#' && line[1] >= '0' && line[1] <= '9') groups++;
    fclose(f);
    return groups;
}

// 用 FIEMAP 取文件的物理区段（extent）
static int extents(const char *p, unsigned long long *first, int *nexts, int *ngroups) {
    int fd = open(p, O_RDONLY);
    if (fd < 0) die(p);
    size_t sz = sizeof(struct fiemap) + 256 * sizeof(struct fiemap_extent);
    struct fiemap *fm = calloc(1, sz);
    fm->fm_length = ~0ULL;
    fm->fm_flags = FIEMAP_FLAG_SYNC;
    fm->fm_extent_count = 256;
    if (ioctl(fd, FS_IOC_FIEMAP, fm) < 0) { close(fd); free(fm); return -1; }
    *nexts = fm->fm_mapped_extents;
    *first = fm->fm_mapped_extents ? fm->fm_extents[0].fe_physical / blksz : 0;
    // 统计这个文件的数据跨越了多少个不同的块组
    long seen[256]; int ns = 0;
    for (unsigned i = 0; i < fm->fm_mapped_extents; i++) {
        unsigned long long b0 = fm->fm_extents[i].fe_physical / blksz;
        unsigned long long b1 = (fm->fm_extents[i].fe_physical + fm->fm_extents[i].fe_length - 1) / blksz;
        for (unsigned long long g = b0 / blocks_per_group; g <= b1 / blocks_per_group; g++) {
            int dup = 0;
            for (int k = 0; k < ns; k++) if (seen[k] == (long)g) dup = 1;
            if (!dup && ns < 256) seen[ns++] = (long)g;
        }
    }
    *ngroups = ns;
    close(fd); free(fm);
    return 0;
}

static void write_file(const char *p, long bytes) {
    int fd = open(p, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) die(p);
    static char buf[1 << 20];
    memset(buf, 'x', sizeof buf);
    while (bytes > 0) {
        long n = bytes > (long)sizeof buf ? (long)sizeof buf : bytes;
        if (write(fd, buf, n) != n) die("write");
        bytes -= n;
    }
    if (fsync(fd) < 0) die("fsync");   // 触发延迟分配（delalloc），让块真正落盘
    close(fd);
}

static void show(const char *p, const char *kind) {
    struct stat st;
    if (stat(p, &st) < 0) die(p);
    long ig = (long)((st.st_ino - 1) / inodes_per_group);
    unsigned long long first = 0; int ne = 0, ng = 0;
    if (extents(p, &first, &ne, &ng) < 0) { printf("%-24s FIEMAP 不支持\n", p); return; }
    printf("%-12s %-4s inode=%-9lu inode组=%-5ld 首块=%-11llu 数据组=%-5llu 区段数=%-3d 跨组数=%d\n",
           p, kind, (unsigned long)st.st_ino, ig, first, first / blocks_per_group, ne, ng);
}

int main(int argc, char **argv) {
    const char *root = argc > 1 ? argv[1] : "playground";
    long bigmb = argc > 2 ? atol(argv[2]) : 64;
    mkdir(root, 0755);
    struct stat st; struct statvfs vf;
    if (stat(root, &st) < 0 || statvfs(root, &vf) < 0) die(root);
    blksz = vf.f_bsize;
    blocks_per_group = 8 * blksz;             // ext4：一个块的位图管 8*blksz 个块
    long groups = count_groups(st.st_dev);
    if (groups <= 0) { fprintf(stderr, "读不到 /proc/fs/ext4/*/mb_groups（不是 ext4？）\n"); return 1; }
    inodes_per_group = (long)(vf.f_files / groups);
    printf("块大小=%ld  每组块数=%ld  块组数=%ld  每组 inode 数=%ld\n\n", blksz, blocks_per_group, groups, inodes_per_group);

    char p[512];
    const char *dirs[] = {"a", "b", "c"};
    for (int d = 0; d < 3; d++) {
        snprintf(p, sizeof p, "%s/%s", root, dirs[d]);
        mkdir(p, 0755);
        show(p, "目录");
        for (int f = 0; f < 3; f++) {
            snprintf(p, sizeof p, "%s/%s/f%d", root, dirs[d], f);
            write_file(p, 8 * 1024);
            show(p, "小");
        }
        snprintf(p, sizeof p, "%s/%s/big", root, dirs[d]);
        write_file(p, bigmb * 1024 * 1024);
        show(p, "大");
        printf("\n");
    }
    // 清理：删掉刚才写的文件（大文件很占空间）。第 3 个参数写 keep 可以保留
    if (!(argc > 3 && !strcmp(argv[3], "keep"))) {
        for (int d = 0; d < 3; d++) {
            for (int f = 0; f < 3; f++) { snprintf(p, sizeof p, "%s/%s/f%d", root, dirs[d], f); unlink(p); }
            snprintf(p, sizeof p, "%s/%s/big", root, dirs[d]); unlink(p);
            snprintf(p, sizeof p, "%s/%s", root, dirs[d]); rmdir(p);
        }
        rmdir(root);
        printf("（已清理 %s；想保留请加第 3 个参数 keep）\n", root);
    }
    return 0;
}
