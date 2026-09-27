CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -pthread
PROGS   = vector_deadlock atomicity

all: $(PROGS)

vector_deadlock: vector_deadlock.c
	$(CC) $(CFLAGS) -o $@ $<

atomicity: atomicity.c
	$(CC) $(CFLAGS) -o $@ $<

# 依次运行四种加锁策略（naive 可能在看门狗处报告死锁并以状态 2 退出）
run: all
	-./vector_deadlock naive 100000
	./vector_deadlock ordered 100000
	./vector_deadlock trylock 100000
	./vector_deadlock global 100000
	./atomicity bug
	./atomicity fixed

clean:
	rm -f $(PROGS)

.PHONY: all run clean
