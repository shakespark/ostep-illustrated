// sparse.c —— 观察 inode 里的"大小"与"已分配块数"是两回事，并观察目录也会占用数据块
// 用法：./sparse [工作目录]   （默认在当前目录下建 fsimpl-tmp/，结束时全部删除）
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static void show(const char *tag, const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) { perror("stat"); exit(1); }
    // st_blocks 的单位固定是 512 字节扇区（与文件系统块大小无关）
    printf("inode=%-9lu size=%-11lld blocks=%-4lld(=%3lld KB) links=%lu  <- %s\n",
           (unsigned long)st.st_ino, (long long)st.st_size, (long long)st.st_blocks,
           (long long)st.st_blocks / 2, (unsigned long)st.st_nlink, tag);
}

int main(int argc, char *argv[]) {
    const char *base = argc > 1 ? argv[1] : ".";
    char dir[4096], file[4200], name[4300];
    snprintf(dir, sizeof dir, "%s/fsimpl-tmp", base);
    mkdir(dir, 0755);
    snprintf(file, sizeof file, "%s/sparse.dat", dir);

    printf("== 1. 文件大小 vs 分配的数据块 ==\n");
    int fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) { perror("open"); return 1; }
    show("刚创建（空文件）", file);
    write(fd, "x", 1); fsync(fd);
    show("写入 1 字节后", file);
    lseek(fd, 12 * 4096, SEEK_SET);            // 跳过 12 个块：刚好越过"直接指针"覆盖的范围
    write(fd, "y", 1); fsync(fd);
    show("在偏移 48KB 处再写 1 字节", file);
    lseek(fd, 1L << 30, SEEK_SET);             // 跳到 1GB 处
    write(fd, "z", 1); fsync(fd);
    show("在偏移 1GB 处再写 1 字节", file);
    close(fd);

    printf("\n== 2. 目录也是文件：条目多了，目录要分配更多数据块 ==\n");
    show("空目录", dir);
    for (int n = 1; n <= 400; n++) {
        snprintf(name, sizeof name, "%s/file_with_a_fairly_long_name_%04d", dir, n);
        int f = open(name, O_CREAT | O_WRONLY, 0644);
        if (f < 0) { perror("open"); return 1; }
        close(f);
        if (n == 50 || n == 100 || n == 200 || n == 400) {
            char tag[64]; snprintf(tag, sizeof tag, "目录里有 %d 个文件后", n + 1);
            show(tag, dir);
        }
    }
    // 清理
    for (int n = 1; n <= 400; n++) {
        snprintf(name, sizeof name, "%s/file_with_a_fairly_long_name_%04d", dir, n);
        unlink(name);
    }
    show("删掉 400 个文件后（目录不收缩）", dir);
    unlink(file);
    rmdir(dir);
    return 0;
}
