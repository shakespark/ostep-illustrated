// server.c —— 原书 Figure 48.1 的服务端：收到消息就回一句 "goodbye world"
// 用法：./server [处理多少条消息后退出，默认永远运行]
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include "udp.h"

int main(int argc, char *argv[]) {
    int limit = (argc > 1) ? atoi(argv[1]) : -1;
    int sd = UDP_Open(10000);
    assert(sd > -1);
    printf("server: listening on UDP port 10000\n");
    fflush(stdout);
    for (int handled = 0; limit < 0 || handled < limit; ) {
        struct sockaddr_in addr;
        char message[BUFFER_SIZE];
        int rc = UDP_Read(sd, &addr, message, BUFFER_SIZE);
        if (rc > 0) {
            printf("server: got \"%s\" (%d bytes) from %s:%d\n", message, rc,
                   inet_ntoa(addr.sin_addr), ntohs(addr.sin_port));
            char reply[BUFFER_SIZE];
            sprintf(reply, "goodbye world");
            rc = UDP_Write(sd, &addr, reply, BUFFER_SIZE);
            printf("server: replied (%d bytes)\n", rc);
            fflush(stdout);
            handled++;
        }
    }
    UDP_Close(sd);
    return 0;
}
