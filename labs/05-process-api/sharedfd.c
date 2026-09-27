// sharedfd.c —— 作业题 2：open 之后 fork，父子共享同一个"打开文件"（包括文件偏移量）
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

int main(void) {
    int fd = open("shared.txt", O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); exit(1); }
    int rc = fork();
    const char *who = (rc == 0) ? "child " : "parent";
    for (int i = 0; i < 5; i++) {
        char buf[64];
        int n = snprintf(buf, sizeof buf, "%s line %d\n", who, i);
        write(fd, buf, n);                 // 每次 write 都从"共享的偏移量"处继续写
        usleep(1000);
    }
    if (rc == 0) exit(0);
    wait(NULL);
    printf("偏移量 lseek(fd,0,SEEK_CUR) = %ld（= 10 行的总字节数，说明父子共用一个偏移量）\n",
           (long) lseek(fd, 0, SEEK_CUR));
    close(fd);
    return 0;
}
