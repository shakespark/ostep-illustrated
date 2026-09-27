// fork-seek.c —— 复刻原书 Figure 39.2，并对比 dup() 与"两次 open()"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>

static off_t cur(int fd) { return lseek(fd, 0, SEEK_CUR); }

int main(void) {
    int f = open("file.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    for (int i = 0; i < 300; i++) write(f, "x", 1);    // 准备一个 300 字节的文件
    close(f);

    printf("== 1. fork()：父子进程共享同一个打开文件表项 ==\n");
    int fd = open("file.txt", O_RDONLY);
    assert(fd >= 0);
    fflush(stdout);                      // 避免 stdio 缓冲区被 fork 复制一份
    int rc = fork();
    if (rc == 0) {
        rc = lseek(fd, 10, SEEK_SET);
        printf("child: offset %d\n", rc);
        return 0;
    } else if (rc > 0) {
        (void) wait(NULL);
        printf("parent: offset %d\n", (int) lseek(fd, 0, SEEK_CUR));
    }
    close(fd);

    printf("\n== 2. dup()：两个 fd，同一个表项，偏移一起动 ==\n");
    char buf[100];
    fd = open("file.txt", O_RDONLY);
    int fd2 = dup(fd);
    printf("fd=%d fd2=%d\n", fd, fd2);
    ssize_t n = read(fd, buf, 100);
    printf("read(fd, 100)  = %zd  -> fd 偏移 %lld, fd2 偏移 %lld\n", n, (long long)cur(fd), (long long)cur(fd2));
    n = read(fd2, buf, 100);
    printf("read(fd2, 100) = %zd  -> fd 偏移 %lld, fd2 偏移 %lld\n", n, (long long)cur(fd), (long long)cur(fd2));
    close(fd); close(fd2);

    printf("\n== 3. 两次 open()：两个独立的表项，偏移各管各的 ==\n");
    int fd1 = open("file.txt", O_RDONLY);
    fd2 = open("file.txt", O_RDONLY);
    printf("fd1=%d fd2=%d\n", fd1, fd2);
    n = read(fd1, buf, 100);
    printf("read(fd1, 100) = %zd  -> fd1 偏移 %lld, fd2 偏移 %lld\n", n, (long long)cur(fd1), (long long)cur(fd2));
    n = read(fd2, buf, 100);
    printf("read(fd2, 100) = %zd  -> fd1 偏移 %lld, fd2 偏移 %lld\n", n, (long long)cur(fd1), (long long)cur(fd2));
    close(fd1); close(fd2);

    printf("\n== 4. dup2() 做输出重定向（shell 的 > 就是这么实现的）==\n");
    int out = open("out.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    int saved = dup(STDOUT_FILENO);
    fflush(stdout);
    dup2(out, STDOUT_FILENO);            // 现在 fd 1 指向 out.txt 的表项
    printf("这行字写进了 out.txt，而不是屏幕\n");
    fflush(stdout);
    dup2(saved, STDOUT_FILENO); close(saved); close(out);
    FILE *fp = fopen("out.txt", "r"); char line[128];
    if (fgets(line, sizeof line, fp)) printf("out.txt 的内容：%s", line);
    fclose(fp);
    unlink("out.txt"); unlink("file.txt");
    return 0;
}
