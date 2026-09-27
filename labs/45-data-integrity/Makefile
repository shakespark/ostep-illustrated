CC ?= gcc
CFLAGS ?= -O2 -Wall -Wextra

all: csum

csum: csum.c
	$(CC) $(CFLAGS) -o $@ $<

run: csum
	./csum

clean:
	rm -f csum
