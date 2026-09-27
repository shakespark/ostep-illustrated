# 第 26 章实验：gcc -Wall -pthread。-O0 以便 objdump 看到与原书一致的"读-加-写"三条指令。
CC     = gcc
CFLAGS = -Wall -O0 -g -pthread

PROGS = t0 t1 t1_lock

all: $(PROGS)

%: %.c common_threads.h
	$(CC) $(CFLAGS) -o $@ $<

# 连续运行 t1 五次，观察每次结果都不同
race: t1
	for i in 1 2 3 4 5; do ./t1 | tail -1; done

# 反汇编，找出 counter = counter + 1 对应的指令
disasm: t1
	objdump -d --no-show-raw-insn t1 | awk '/<mythread>:/,/ret/'

# 把 t1 绑定到单个 CPU（taskset -c 0）上运行：只能靠定时器中断交错，出错概率低很多
single: t1
	for i in 1 2 3 4 5; do taskset -c 0 ./t1 | tail -1; done

clean:
	rm -f $(PROGS)

.PHONY: all race disasm single clean
