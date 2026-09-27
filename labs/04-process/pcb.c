// pcb.c —— Linux 里每个进程的 "PCB"（task_struct）有一部分通过 /proc/<pid>/ 暴露出来。
// 本程序打开一个文件、fork 一个子进程，然后打印两者的 状态 / PID / 父 PID / 上下文切换次数 / 打开的文件。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/wait.h>

static void show(const char *who) {
    char path[64], line[256];
    printf("---- %s (pid %d) ----\n", who, getpid());
    snprintf(path, sizeof path, "/proc/%d/status", getpid());
    FILE *f = fopen(path, "r");
    while (f && fgets(line, sizeof line, f)) {
        if (!strncmp(line, "State:", 6) || !strncmp(line, "Pid:", 4) || !strncmp(line, "PPid:", 5) ||
            !strncmp(line, "VmSize:", 7) || !strncmp(line, "voluntary_ctxt", 14) ||
            !strncmp(line, "nonvoluntary_ctxt", 17))
            printf("  %s", line);
    }
    if (f) fclose(f);
    DIR *d = opendir("/proc/self/fd");       // 打开文件表 ofile[]
    struct dirent *e;
    while (d && (e = readdir(d))) {
        if (e->d_name[0] == '.') continue;
        char p[300], target[256];
        snprintf(p, sizeof p, "/proc/self/fd/%s", e->d_name);
        ssize_t n = readlink(p, target, sizeof target - 1);
        if (n < 0) continue;
        target[n] = 0;
        printf("  fd %-2s -> %s\n", e->d_name, target);
    }
    if (d) closedir(d);
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    int fd = open("/tmp/pcb-demo.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    usleep(1000);                            // 睡一下：主动让出 CPU，voluntary_ctxt_switches 会加 1
    pid_t rc = fork();
    if (rc == 0) {
        show("child ");
        exit(0);
    }
    wait(NULL);
    show("parent");
    close(fd);
    return 0;
}
