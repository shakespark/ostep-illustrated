CC     = gcc
CFLAGS = -Wall -Wextra -O2

all: server client reliable

server: server.c udp.c udp.h
	$(CC) $(CFLAGS) -o $@ server.c udp.c

client: client.c udp.c udp.h
	$(CC) $(CFLAGS) -o $@ client.c udp.c

reliable: reliable.c udp.c udp.h
	$(CC) $(CFLAGS) -o $@ reliable.c udp.c

# 在本机跑一次原书的 hello/goodbye：后台启动服务端（处理 1 条消息后退出），再运行客户端
run: server client
	./server 1 & sleep 0.3; ./client localhost; wait

clean:
	rm -f server client reliable

.PHONY: all run clean
