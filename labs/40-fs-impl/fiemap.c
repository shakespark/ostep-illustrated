// fiemap.c —— 用 FIEMAP ioctl 看文件的"逻辑块 → 物理块"映射（类似 filefrag -v）
// ext4 用 extent（起始物理块 + 长度）而不是逐块指针来记录这张映射表
// 用法：./fiemap 文件名      不给参数则自己造一个带"洞"的测试文件
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <linux/fs.h>
#include <linux/fiemap.h>

static int dump(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror("open"); return 1; }
    struct stat st; fstat(fd, &st);
    size_t n = 64;
    struct fiemap *fm = calloc(1, sizeof *fm + n * sizeof(struct fiemap_extent));
    fm->fm_start = 0;
    fm->fm_length = ~0ULL;
    fm->fm_flags = FIEMAP_FLAG_SYNC;           // 先把脏数据刷盘，保证映射已确定
    fm->fm_extent_count = n;
    if (ioctl(fd, FS_IOC_FIEMAP, fm) < 0) { perror("ioctl(FS_IOC_FIEMAP)"); return 1; }
    printf("%s: size=%lld 字节, inode=%lu, extent 数=%u\n", path, (long long)st.st_size,
           (unsigned long)st.st_ino, fm->fm_mapped_extents);
    printf("  %-3s %-18s %-18s %-10s %s\n", "#", "逻辑偏移(4K块)", "物理位置(4K块)", "长度(块)", "标志");
    for (unsigned i = 0; i < fm->fm_mapped_extents; i++) {
        struct fiemap_extent *e = &fm->fm_extents[i];
        printf("  %-3u %-18llu %-18llu %-10llu %s%s\n", i,
               (unsigned long long)(e->fe_logical / 4096), (unsigned long long)(e->fe_physical / 4096),
               (unsigned long long)(e->fe_length / 4096),
               (e->fe_flags & FIEMAP_EXTENT_LAST) ? "LAST " : "",
               (e->fe_flags & FIEMAP_EXTENT_UNWRITTEN) ? "UNWRITTEN" : "");
    }
    free(fm); close(fd);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1) return dump(argv[1]);
    const char *path = "fiemap-test.dat";
    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) { perror("open"); return 1; }
    char buf[4096]; memset(buf, 'a', sizeof buf);
    for (int i = 0; i < 256; i++) write(fd, buf, sizeof buf);   // 逻辑块 0..255：1MB 连续数据
    lseek(fd, 1024L * 4096, SEEK_SET);                          // 留一个洞：逻辑块 256..1023 不分配
    for (int i = 0; i < 16; i++) write(fd, buf, sizeof buf);    // 逻辑块 1024..1039
    fsync(fd); close(fd);
    int rc = dump(path);
    unlink(path);
    return rc;
}
