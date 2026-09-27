CC     = gcc
CFLAGS = -Wall -O1 -g
PROGS  = va va2 va-fork memory-user

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

# 连续运行两次 va：开启 ASLR 时两次地址不同；用 setarch -R 关闭随机化后两次相同
run: all
	./va; echo; ./va; echo
	setarch $$(uname -m) -R ./va; echo; setarch $$(uname -m) -R ./va; echo
	./va2
	./va-fork

clean:
	rm -f $(PROGS)

.PHONY: all run clean
