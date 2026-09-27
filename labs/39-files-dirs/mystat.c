// mystat.c —— 自己实现一个 stat：打印 struct stat 的各个字段
// 用法：./mystat 路径...      不给参数时做一个演示：目录的 link count 随子目录增加
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>

static void perm(mode_t m, char *out) {
    out[0] = S_ISDIR(m) ? 'd' : S_ISLNK(m) ? 'l' : '-';
    const char *rwx = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) out[i + 1] = (m & (0400 >> i)) ? rwx[i] : '-';
    out[10] = 0;
}
static void tm(const char *label, struct timespec t) {
    char b[64]; strftime(b, sizeof b, "%Y-%m-%d %H:%M:%S", localtime(&t.tv_sec));
    printf("%s: %s.%09ld\n", label, b, t.tv_nsec);
}
static int show(const char *path) {
    struct stat s;
    if (lstat(path, &s) != 0) { perror(path); return 1; }
    char p[11]; perm(s.st_mode, p);
    printf("  File: %s\n", path);
    printf("  Size: %-10lld Blocks: %-6lld IO Block: %-6ld %s\n", (long long)s.st_size, (long long)s.st_blocks,
           (long)s.st_blksize, S_ISDIR(s.st_mode) ? "directory" : S_ISLNK(s.st_mode) ? "symbolic link" : "regular file");
    printf("Device: %u,%u  Inode: %-10lu Links: %lu\n", major(s.st_dev), minor(s.st_dev), (unsigned long)s.st_ino, (unsigned long)s.st_nlink);
    printf("Access: (%04o/%s)  Uid: %u  Gid: %u\n", s.st_mode & 07777, p, s.st_uid, s.st_gid);
    tm("Access", s.st_atim); tm("Modify", s.st_mtim); tm("Change", s.st_ctim);
    return 0;
}
static unsigned long nlink(const char *p) { struct stat s; stat(p, &s); return s.st_nlink; }

int main(int argc, char *argv[]) {
    if (argc > 1) { int rc = 0; for (int i = 1; i < argc; i++) rc |= show(argv[i]); return rc; }
    int fd = open("file", O_CREAT | O_WRONLY | O_TRUNC, 0640); write(fd, "hello\n", 6); close(fd);
    printf("$ echo hello > file; ./mystat file\n");
    show("file");
    unlink("file");

    printf("\n== 目录的 link count = 2 + 子目录个数 ==\n");
    mkdir("d", 0755);
    printf("mkdir d            -> d 的 links = %lu （父目录里的 \"d\" + 自己的 \".\"）\n", nlink("d"));
    fd = open("d/f", O_CREAT | O_WRONLY, 0644); close(fd);
    printf("touch d/f          -> d 的 links = %lu （普通文件不影响）\n", nlink("d"));
    char name[32];
    for (int i = 1; i <= 3; i++) {
        snprintf(name, sizeof name, "d/sub%d", i); mkdir(name, 0755);
        printf("mkdir d/sub%d       -> d 的 links = %lu （sub%d/.. 指向 d）\n", i, nlink("d"), i);
    }
    for (int i = 1; i <= 3; i++) { snprintf(name, sizeof name, "d/sub%d", i); rmdir(name); }
    unlink("d/f");
    printf("rmdir d            -> %s\n", rmdir("d") == 0 ? "成功（此时只剩 . 和 ..）" : "失败");
    return 0;
}
