CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread
PROGS = fork_join pc dining barrier throttle

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS)

.PHONY: all clean
