// links.c —— 硬链接 vs 符号链接：inode 号、link count、符号链接的大小、悬空引用
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

static void st(const char *name) {
    struct stat s;
    if (lstat(name, &s) != 0) { printf("  %-18s lstat 失败（不存在）\n", name); return; }
    const char *type = S_ISLNK(s.st_mode) ? "symbolic link" : S_ISDIR(s.st_mode) ? "directory" : "regular file";
    printf("  %-18s inode=%-9lu links=%lu size=%-3lld %s", name, (unsigned long)s.st_ino,
           (unsigned long)s.st_nlink, (long long)s.st_size, type);
    if (S_ISLNK(s.st_mode)) {
        char target[256]; ssize_t n = readlink(name, target, sizeof target - 1);
        target[n > 0 ? n : 0] = 0;
        printf(" -> %s", target);
    }
    printf("\n");
}
static void cat(const char *name) {
    char buf[64]; int fd = open(name, O_RDONLY);
    if (fd < 0) { printf("  cat %s: 打开失败（No such file or directory）\n", name); return; }
    ssize_t n = read(fd, buf, sizeof buf - 1); buf[n > 0 ? n : 0] = 0; close(fd);
    printf("  cat %s: %s", name, buf);
}

int main(void) {
    printf("== 硬链接：link() 只是在目录里再加一个名字，指向同一个 inode ==\n");
    int fd = open("file", O_CREAT | O_WRONLY | O_TRUNC, 0644); write(fd, "hello\n", 6); close(fd);
    printf("echo hello > file\n"); st("file");
    link("file", "file2");  printf("ln file file2\n");  st("file"); st("file2");
    link("file2", "file3"); printf("ln file2 file3\n"); st("file");
    unlink("file");  printf("rm file\n");  st("file2");
    unlink("file2"); printf("rm file2\n"); st("file3"); cat("file3");
    unlink("file3"); printf("rm file3   （link count 降到 0，inode 和数据块才真正被释放）\n\n");

    printf("== 符号链接：一个独立的小文件，内容是目标的路径名 ==\n");
    fd = open("file", O_CREAT | O_WRONLY | O_TRUNC, 0644); write(fd, "hello\n", 6); close(fd);
    symlink("file", "file2");                    printf("ln -s file file2\n"); st("file"); st("file2"); cat("file2");
    fd = open("alongerfilename", O_CREAT | O_WRONLY | O_TRUNC, 0644); write(fd, "hello\n", 6); close(fd);
    symlink("alongerfilename", "file3");         printf("ln -s alongerfilename file3\n"); st("file3");
    unlink("file");                              printf("rm file   （符号链接不增加 link count，也拦不住删除）\n");
    st("file2"); cat("file2");
    printf("  → file2 成了悬空引用（dangling reference）\n");
    unlink("file2"); unlink("file3"); unlink("alongerfilename");
    return 0;
}
