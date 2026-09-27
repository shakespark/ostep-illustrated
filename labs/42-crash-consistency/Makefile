# 第 42 章实验：崩溃一致性。gcc -Wall -O2
CC     = gcc
CFLAGS = -Wall -Wextra -O2 -g

all: journal

journal: journal.c
	$(CC) $(CFLAGS) -o $@ $<

# 穷举 4 种写协议 × 所有崩溃点，恢复后检查文件系统状态
crash: journal
	./journal crashtest

# 测量 fsync 的真实开销和应用层原子更新（rename）
bench: journal
	./journal fsyncbench

clean:
	rm -f journal disk.img bench.dat config.txt config.tmp

.PHONY: all crash bench clean
