CC      = gcc
CFLAGS  = -O2 -Wall -pthread

all: counters counters-nopad

counters: counters.c
	$(CC) $(CFLAGS) -o $@ $<

# 不做缓存行填充的版本：local[] 紧挨着放，观察伪共享（false sharing）
counters-nopad: counters.c
	$(CC) $(CFLAGS) -DNOPAD -o $@ $<

run: counters
	./counters

nopad: counters-nopad
	./counters-nopad

clean:
	rm -f counters counters-nopad

.PHONY: all run nopad clean
