// echo_server.c —— 用 select() 实现的单线程多客户端 echo 服务器
// 用法: ./echo_server [-p 端口] [-b 毫秒]
//   -p  监听端口（默认 9090，只绑定 127.0.0.1）
//   -b  每处理一条消息时故意调用阻塞的 usleep(毫秒)，模拟"事件处理器里做了阻塞 I/O"
//       用来观察：一个事件处理器阻塞，整个事件循环（所有客户端）都跟着停下
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char *argv[]) {
    int port = 9090, block_ms = 0, opt;
    while ((opt = getopt(argc, argv, "p:b:")) != -1) {
        if (opt == 'p') port = atoi(optarg);
        else if (opt == 'b') block_ms = atoi(optarg);
        else { fprintf(stderr, "usage: %s [-p port] [-b block_ms]\n", argv[0]); return 1; }
    }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("bind"); return 1; }
    if (listen(lfd, 16) < 0) { perror("listen"); return 1; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    printf("[server] 监听 127.0.0.1:%d，阻塞模拟 = %d ms，listen fd = %d\n", port, block_ms, lfd);

    fd_set all;                 // 我们关心的全部 fd（每轮复制一份交给 select）
    FD_ZERO(&all);
    FD_SET(lfd, &all);
    int maxfd = lfd;
    double t0 = now_ms();
    long rounds = 0;

    while (1) {                                   // ← 事件循环
        fd_set readFDs = all;                     // select 会改写传入的集合，所以每轮重新复制
        int rc = select(maxfd + 1, &readFDs, NULL, NULL, NULL);   // timeout = NULL：一直等
        if (rc < 0) { if (errno == EINTR) continue; perror("select"); return 1; }
        rounds++;
        printf("[server %7.1fms] select 第 %ld 轮返回 rc=%d，就绪:", now_ms() - t0, rounds, rc);
        for (int fd = 0; fd <= maxfd; fd++) if (FD_ISSET(fd, &readFDs)) printf(" fd%d", fd);
        printf("\n");

        for (int fd = 0; fd <= maxfd; fd++) {     // FD_ISSET 逐个检查
            if (!FD_ISSET(fd, &readFDs)) continue;
            if (fd == lfd) {                      // 监听 socket 可读 = 有新连接
                int cfd = accept(lfd, NULL, NULL);
                if (cfd < 0) continue;
                FD_SET(cfd, &all);
                if (cfd > maxfd) maxfd = cfd;
                printf("[server %7.1fms]   accept → 新连接 fd%d\n", now_ms() - t0, cfd);
            } else {                              // 客户端 socket 可读 = 有数据或对端关闭
                char buf[512];
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                if (n <= 0) {
                    printf("[server %7.1fms]   fd%d 关闭\n", now_ms() - t0, fd);
                    close(fd);
                    FD_CLR(fd, &all);
                    continue;
                }
                buf[n] = '\0';
                if (block_ms > 0) usleep(block_ms * 1000);   // ✗ 事件处理器里的阻塞调用
                ssize_t w = write(fd, buf, n);               // echo 回去
                (void)w;
                buf[strcspn(buf, "\n")] = '\0';
                printf("[server %7.1fms]   fd%d 收到 \"%s\" 并回显\n", now_ms() - t0, fd, buf);
            }
        }
    }
}
