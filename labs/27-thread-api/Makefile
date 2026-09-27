CC      = gcc
CFLAGS  = -Wall -O1 -g -pthread
PROGS   = create_args bad_return bad_return_asan cond_wait cond_wait_tsan

all: $(PROGS)

create_args: create_args.c common_threads.h
	$(CC) $(CFLAGS) -o $@ create_args.c

# 普通编译：悬空指针读"可能"读到垃圾，也可能碰巧读到 1 2
bad_return: bad_return.c common_threads.h
	$(CC) $(CFLAGS) -o $@ bad_return.c

# AddressSanitizer 编译：运行时捕获 stack-use-after-return
bad_return_asan: bad_return.c common_threads.h
	$(CC) -Wall -O0 -g -pthread -fsanitize=address -fno-omit-frame-pointer -o $@ bad_return.c

cond_wait: cond_wait.c common_threads.h
	$(CC) $(CFLAGS) -o $@ cond_wait.c

# ThreadSanitizer 编译：报告裸 flag 自旋那一段的数据竞争
cond_wait_tsan: cond_wait.c common_threads.h
	$(CC) $(CFLAGS) -fsanitize=thread -o $@ cond_wait.c

run: all
	./create_args
	./bad_return || true
	./bad_return helper || true
	ASAN_OPTIONS=detect_stack_use_after_return=1 ./bad_return_asan || true
	ASAN_OPTIONS=detect_stack_use_after_return=1 ./bad_return_asan helper || true
	./cond_wait

clean:
	rm -f $(PROGS)

.PHONY: all run clean
