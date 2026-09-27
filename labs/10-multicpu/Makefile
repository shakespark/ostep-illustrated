CC     = gcc
CFLAGS = -O2 -Wall

all: affinity

affinity: affinity.c
	$(CC) $(CFLAGS) -o $@ $<

run: affinity
	./affinity

clean:
	rm -f affinity

.PHONY: all run clean
