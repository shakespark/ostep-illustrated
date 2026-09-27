CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
PROGS = pc join alloc

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
