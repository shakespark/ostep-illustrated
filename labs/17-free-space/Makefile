CC     = gcc
CFLAGS = -Wall -O1 -g
PROGS  = freelist glibc_header

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./freelist
	./glibc_header

clean:
	rm -f $(PROGS)

.PHONY: all run clean
