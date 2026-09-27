CC     = gcc
CFLAGS = -O2 -Wall -Wextra -std=gnu11
PROGS  = sparse fiemap cache

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS) cache-test.dat fiemap-test.dat
	rm -rf fsimpl-tmp

.PHONY: all clean
