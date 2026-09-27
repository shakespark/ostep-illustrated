CC     = gcc
CFLAGS = -O2 -Wall -Wextra -std=gnu11

all: seqrand

seqrand: seqrand.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f seqrand seqrand-test.dat

.PHONY: all clean
