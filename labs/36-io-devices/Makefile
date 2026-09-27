CC = gcc
CFLAGS = -O2 -Wall -Wextra

all: wait-io

wait-io: wait-io.c
	$(CC) $(CFLAGS) -o $@ $<

run: wait-io
	./wait-io poll 500
	./wait-io block 500
	./wait-io hybrid 500 100
	@echo "--- 快设备（1ms）---"
	./wait-io poll 1
	./wait-io block 1

clean:
	rm -f wait-io

.PHONY: all run clean
