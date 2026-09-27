// client.c —— 原书 Figure 48.1 的客户端：发 "hello world"，等待回复
// 用法：./client [主机名，默认 localhost]
#include <stdio.h>
#include <arpa/inet.h>
#include "udp.h"

int main(int argc, char *argv[]) {
    char *host = (argc > 1) ? argv[1] : "localhost";
    int sd = UDP_Open(20000);
    struct sockaddr_in addrSnd, addrRcv;
    int rc = UDP_FillSockAddr(&addrSnd, host, 10000);
    if (rc != 0) { fprintf(stderr, "client: cannot resolve %s\n", host); return 1; }
    char message[BUFFER_SIZE];
    sprintf(message, "hello world");
    printf("client: sending \"%s\" to %s:10000\n", message, host);
    fflush(stdout);
    rc = UDP_Write(sd, &addrSnd, message, BUFFER_SIZE);
    if (rc > 0) {
        // 注意：这里会一直阻塞——如果请求或回复丢了，客户端永远等下去（UDP 不可靠）
        rc = UDP_Read(sd, &addrRcv, message, BUFFER_SIZE);
        printf("client: got reply \"%s\" (%d bytes) from %s:%d\n", message, rc,
               inet_ntoa(addrRcv.sin_addr), ntohs(addrRcv.sin_port));
    }
    UDP_Close(sd);
    return 0;
}
