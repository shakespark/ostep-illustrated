// atomic-write.c —— 原子地更新一个文件：写临时文件 → fsync → rename → fsync 目录
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

static void show(const char *path) {
    char buf[128] = {0}; int fd = open(path, O_RDONLY);
    if (fd < 0) { printf("    %s: (不存在)\n", path); return; }
    ssize_t n = read(fd, buf, sizeof buf - 1); close(fd);
    printf("    %s: \"", path);
    for (ssize_t i = 0; i < n; i++) { if (buf[i] == '\n') fputs("\\n", stdout); else putchar(buf[i]); }
    printf("\"\n");
}

int main(void) {
    // 旧版本
    int fd = open("foo.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    write(fd, "line 1\nline 3\n", 14); close(fd);
    printf("初始状态：\n"); show("foo.txt");

    const char *newv = "line 1\nline 2 (inserted)\nline 3\n";
    printf("\n1. open(\"foo.txt.tmp\", O_WRONLY|O_CREAT|O_TRUNC)\n");
    fd = open("foo.txt.tmp", O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    printf("   = %d\n", fd);
    ssize_t w = write(fd, newv, strlen(newv));
    printf("2. write(fd, 新内容, %zu) = %zd\n", strlen(newv), w);
    printf("   此刻崩溃：foo.txt 仍是完整的旧版本，tmp 文件可能残缺——但没人会读它\n");
    show("foo.txt");
    int rc = fsync(fd);
    printf("3. fsync(fd) = %d   （新内容真正落盘后才继续）\n", rc);
    rc = close(fd);
    printf("4. close(fd) = %d\n", rc);
    rc = rename("foo.txt.tmp", "foo.txt");
    printf("5. rename(\"foo.txt.tmp\", \"foo.txt\") = %d   （原子地把新文件换上，旧文件同时消失）\n", rc);
    int dfd = open(".", O_RDONLY);
    rc = fsync(dfd);
    printf("6. fsync(目录 \".\") = %d   （让\"改名\"这一目录修改本身也持久化）\n", rc);
    close(dfd);
    printf("\n最终状态（任何时刻崩溃，foo.txt 要么是旧版、要么是新版）：\n");
    show("foo.txt"); show("foo.txt.tmp");
    unlink("foo.txt");
    return 0;
}
