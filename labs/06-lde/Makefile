# 第 6 章实验：测量系统调用与上下文切换的开销（原书 Homework (Measurement)）
CC     = gcc
CFLAGS = -Wall -O2 -g

PROGS = timer syscall ctxsw

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./timer; echo; ./syscall; echo; ./ctxsw 200000 0; echo; ./ctxsw 200000 -1

clean:
	rm -f $(PROGS)

.PHONY: all run clean
