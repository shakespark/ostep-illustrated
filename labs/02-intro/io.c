// io.c —— 原书 Figure 2.6：open / write / close 三个系统调用写出 /tmp/file
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>

int main(int argc, char *argv[]) {
    int fd = open("/tmp/file", O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU);
    assert(fd > -1);
    int rc = write(fd, "hello world\n", 12);
    assert(rc == 12);
    close(fd);
    return 0;
}
