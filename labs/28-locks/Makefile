# 第 28 章实验：用 GCC __sync/__atomic 内建实现 TAS、CAS、ticket 锁，与 pthread_mutex 对比。
CC     = gcc
CFLAGS = -Wall -O2 -pthread

LOOPS ?= 1000000

all: locks

locks: locks.c
	$(CC) $(CFLAGS) -o $@ $<

# 正确性：错误的 flag 锁 vs 不加锁 vs 各种正确的锁（4 线程）
correct: locks
	@for l in none flag tas cas ticket mutex; do ./locks $$l 4 $(LOOPS); done

# 性能：线程数 1 2 4 8 16 下各种锁的耗时（每个运行最多 20 秒）
bench: locks
	@for n in 1 2 4 8 16; do \
	  for l in tas ttas cas ticket yield mutex; do ./locks $$l $$n $(LOOPS) 20; done; echo; done

# 超额订阅：线程数 = 2 × CPU 数，持锁者/下一个排队者可能被抢占
oversub: locks
	@N=$$(( $$(nproc) * 2 )); for l in tas ttas ticket yield mutex; do ./locks $$l $$N 200000 10; done

# 单处理器：把所有线程钉在 CPU 0 上
single: locks
	@for l in tas ticket yield mutex; do taskset -c 0 ./locks $$l 4 5000000 5; done

clean:
	rm -f locks

.PHONY: all correct bench oversub single clean
