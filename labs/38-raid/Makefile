CC     = gcc
CFLAGS = -O2 -Wall -Wextra -std=gnu11

all: raid

raid: raid.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f raid

.PHONY: all clean
