# 第 5 章实验：原书 p1–p4 + 作业题里的几个小程序
CC     = gcc
CFLAGS = -Wall -O0 -g

PROGS = p1 p2 p3 p4 var sharedfd pipe

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

# 连续运行 p1 二十次，统计 fork 之后父/子谁先打印（stdbuf -oL：行缓冲，避免下面的"缓冲区陷阱"）
order: p1
	for i in $$(seq 20); do stdbuf -oL ./p1 | sed -n 2p | cut -d' ' -f1; done | sort | uniq -c

# 同上，但把 p1 绑在一个 CPU 上：父子只能轮流运行
order1: p1
	for i in $$(seq 20); do taskset -c 0 stdbuf -oL ./p1 | sed -n 2p | cut -d' ' -f1; done | sort | uniq -c

# 缓冲区陷阱：输出到管道时 stdout 是全缓冲，fork 会把没刷出去的 "hello" 复制一份
buffer: p1
	./p1 | cat

redirect: p4
	./p4 && cat p4.output

clean:
	rm -f $(PROGS) p4.output shared.txt

.PHONY: all order order1 buffer redirect clean
