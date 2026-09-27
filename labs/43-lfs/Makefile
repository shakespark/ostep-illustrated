CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

all: seqbuf

seqbuf: seqbuf.c
	$(CC) $(CFLAGS) -o $@ $<

run: seqbuf
	./seqbuf
	python3 lfs_mini.py -L "c,/foo:w,/foo,0,1" -c
	python3 lfs_mini.py --demo-clean

clean:
	rm -f seqbuf lfs_test.dat

.PHONY: all run clean
