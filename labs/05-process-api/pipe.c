// pipe.c —— 作业题 8：创建两个子进程，用 pipe() 把第一个的标准输出接到第二个的标准输入
// 相当于 shell 里的：  ls -1 /  |  wc -l
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int p[2];
    if (pipe(p) < 0) { perror("pipe"); exit(1); }   // p[0] 读端，p[1] 写端

    pid_t a = fork();
    if (a == 0) {                     // 第一个孩子：ls
        dup2(p[1], STDOUT_FILENO);    // fd 1 → 管道写端
        close(p[0]); close(p[1]);
        execlp("ls", "ls", "-1", "/", (char *) NULL);
        perror("exec ls"); exit(1);
    }
    pid_t b = fork();
    if (b == 0) {                     // 第二个孩子：wc
        dup2(p[0], STDIN_FILENO);     // fd 0 → 管道读端
        close(p[0]); close(p[1]);
        execlp("wc", "wc", "-l", (char *) NULL);
        perror("exec wc"); exit(1);
    }
    close(p[0]); close(p[1]);         // 父进程必须关掉两端，否则 wc 永远等不到 EOF
    waitpid(a, NULL, 0);
    waitpid(b, NULL, 0);
    fprintf(stderr, "(父进程 %d：两个孩子 %d 和 %d 都结束了)\n", getpid(), a, b);
    return 0;
}
