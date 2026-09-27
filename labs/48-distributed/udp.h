// udp.h —— 原书 Figure 48.2 的简单 UDP 库（接口声明）
#ifndef UDP_H
#define UDP_H
#include <netinet/in.h>

#define BUFFER_SIZE 1000

int UDP_Open(int port);                                   // 创建并绑定 UDP 套接字
int UDP_FillSockAddr(struct sockaddr_in *addr, char *hostname, int port);
int UDP_Write(int sd, struct sockaddr_in *addr, char *buffer, int n);
int UDP_Read(int sd, struct sockaddr_in *addr, char *buffer, int n);
int UDP_Close(int sd);
#endif
