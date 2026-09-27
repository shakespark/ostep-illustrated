// mycat.c —— 没有 strace 时的替代品：自己做 cat 的系统调用序列，并按 strace 的格式打印每一步
// 用法：./mycat [文件]   不给参数时自动创建内容为 "hello\n" 的文件 foo，结束后删除
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// 把缓冲区内容按 strace 的风格转义成 "hello\n"
static void show(const char *buf, ssize_t n) {
    fputc('"', stderr);
    for (ssize_t i = 0; i < n && i < 32; i++) {
        if (buf[i] == '\n') fputs("\\n", stderr);
        else if (buf[i] == '"') fputs("\\\"", stderr);
        else fputc(buf[i], stderr);
    }
    fputs(n > 32 ? "\"..." : "\"", stderr);
}

int main(int argc, char *argv[]) {
    const char *path = argc > 1 ? argv[1] : "foo";
    int made = 0;
    if (argc == 1) {                                  // 相当于 echo hello > foo
        int f = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
        write(f, "hello\n", 6);
        close(f);
        made = 1;
    }
    char buf[4096];
    int fd = open(path, O_RDONLY);
    fprintf(stderr, "openat(AT_FDCWD, \"%s\", O_RDONLY) = %d\n", path, fd);
    if (fd < 0) { perror("open"); return 1; }
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        fprintf(stderr, "read(%d, ", fd); show(buf, n > 0 ? n : 0); fprintf(stderr, ", %zu) = %zd\n", sizeof buf, n);
        if (n <= 0) break;
        ssize_t w = write(STDOUT_FILENO, buf, n);   // 真正输出到标准输出（fd 1）
        fprintf(stderr, "write(1, "); show(buf, n); fprintf(stderr, ", %zd) = %zd\n", n, w);
    }
    int rc = close(fd);
    fprintf(stderr, "close(%d) = %d\n", fd, rc);
    if (made) unlink(path);
    return 0;
}
