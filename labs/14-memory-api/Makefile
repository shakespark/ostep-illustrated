# 每个程序编译两份：
#   xxx       —— 普通编译（-g 保留调试信息），观察"不带工具"时的表现
#   xxx-asan  —— 加 -fsanitize=address（AddressSanitizer，含 LeakSanitizer）
# 若装了 valgrind，也可以直接：valgrind --leak-check=yes ./xxx
CC      = gcc
CFLAGS  = -g -O0 -Wall -Wno-uninitialized -Wno-maybe-uninitialized -Wno-free-nonheap-object -Wno-use-after-free -Wno-array-bounds -Wno-stringop-overflow
ASAN    = -fsanitize=address -fno-omit-frame-pointer
PROGS   = null forget_alloc too_small overflow uninit leak dangling double_free bad_free

all: $(PROGS) $(PROGS:=-asan)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

%-asan: %.c
	$(CC) $(CFLAGS) $(ASAN) -o $@ $<

# 逐个运行两种版本，只显示每个输出的前几行
run: all
	@for p in $(PROGS); do \
	  echo "===== ./$$p ====="; ./$$p 2>&1 | head -5; echo "(exit $$?)"; \
	  echo "===== ./$$p-asan ====="; ./$$p-asan 2>&1 | grep -E 'ERROR|SUMMARY|^(READ|WRITE)|located|Direct leak|dst|sum|wrote|data=|exiting|freed|freeing|about' | head -8; \
	done

clean:
	rm -f $(PROGS) $(PROGS:=-asan)

.PHONY: all run clean
