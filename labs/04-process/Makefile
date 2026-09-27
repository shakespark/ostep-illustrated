# 第 4 章实验：进程状态、地址空间布局、Linux 里的 PCB
CC     = gcc
CFLAGS = -Wall -O0 -g

PROGS = states layout pcb

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./states; echo; ./layout; echo; ./pcb

clean:
	rm -f $(PROGS) /tmp/pcb-demo.txt

.PHONY: all run clean
