# 第 2 章实验：原书四个示例程序（cpu / mem / threads / io）
CC     = gcc
CFLAGS = -Wall -O0 -g

PROGS = cpu mem threads io

all: $(PROGS)

cpu: cpu.c common.h
	$(CC) $(CFLAGS) -o $@ $<
mem: mem.c common.h
	$(CC) $(CFLAGS) -o $@ $<
threads: threads.c common.h common_threads.h
	$(CC) $(CFLAGS) -pthread -o $@ $<
io: io.c
	$(CC) $(CFLAGS) -o $@ $<

# 把 4 个 cpu 程序都绑在 0 号 CPU 上跑 4 秒（模拟"只有一个 CPU"）
cpu4: cpu
	taskset -c 0 ./cpu A & taskset -c 0 ./cpu B & taskset -c 0 ./cpu C & taskset -c 0 ./cpu D & \
	sleep 4.5; kill %1 %2 %3 %4 2>/dev/null; pkill -x cpu; true

# 两个 mem 同时跑 3 秒；setarch -R 关闭地址空间随机化（ASLR），复现原书"地址相同"
mem2: mem
	setarch $$(uname -m) -R ./mem & setarch $$(uname -m) -R ./mem & \
	sleep 3.5; pkill -x mem; true

# 同样两个 mem，但保留 ASLR：两次打印的地址通常不同
mem2-aslr: mem
	./mem & ./mem & sleep 2.5; pkill -x mem; true

# loops 从小到大各跑几次，观察何时开始"算错"
race: threads
	for n in 1000 10000 100000 1000000; do for i in 1 2 3; do printf "loops=%-8s " $$n; ./threads $$n | tail -1; done; done

io-run: io
	./io && cat /tmp/file && ls -l /tmp/file

clean:
	rm -f $(PROGS)

.PHONY: all cpu4 mem2 mem2-aslr race io-run clean
