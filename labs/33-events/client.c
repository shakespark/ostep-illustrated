// client.c —— echo 客户端：连接服务器，发送若干条消息，测量每条的往返时间（RTT）
// 用法: ./client 名字 [-p 端口] [-n 条数] [-i 间隔毫秒]
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static double now_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { fprintf(stderr, "usage: %s name [-p port] [-n count] [-i interval_ms]\n", argv[0]); return 1; }
    const char *name = argv[1];
    int port = 9090, count = 3, interval = 100, opt;
    optind = 2;
    while ((opt = getopt(argc, argv, "p:n:i:")) != -1) {
        if (opt == 'p') port = atoi(optarg);
        else if (opt == 'n') count = atoi(optarg);
        else if (opt == 'i') interval = atoi(optarg);
    }
    int sd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sd, (struct sockaddr *)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }
    double total = 0, worst = 0;
    for (int i = 0; i < count; i++) {
        char msg[128], buf[128];
        int len = snprintf(msg, sizeof(msg), "%s#%d\n", name, i);
        double t = now_ms();
        if (write(sd, msg, len) != len) { perror("write"); return 1; }
        ssize_t got = 0;
        while (got < len) {                       // 读满整条回显
            ssize_t n = read(sd, buf + got, sizeof(buf) - 1 - got);
            if (n <= 0) { perror("read"); return 1; }
            got += n;
        }
        double rtt = now_ms() - t;
        total += rtt;
        if (rtt > worst) worst = rtt;
        usleep(interval * 1000);
    }
    printf("[client %s] %d 条消息，平均 RTT = %.2f ms，最大 RTT = %.2f ms\n", name, count, total / count, worst);
    close(sd);
    return 0;
}
