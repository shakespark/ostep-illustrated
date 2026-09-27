# 第 44 章实验：FTL 模拟器（direct / log / log+wl）
CC     = gcc
CFLAGS = -Wall -Wextra -O2

all: ftlsim

ftlsim: ftlsim.c
	$(CC) $(CFLAGS) -o $@ $<

run: ftlsim
	./ftlsim

sweep: ftlsim
	./ftlsim -op

clean:
	rm -f ftlsim

.PHONY: all run sweep clean
