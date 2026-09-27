CC      = gcc
CFLAGS  = -O2 -Wall -Wextra

all: tlb timer

tlb: tlb.c
	$(CC) $(CFLAGS) -o $@ $<

timer: timer.c
	$(CC) $(CFLAGS) -o $@ $<

run: tlb
	./run.sh

clean:
	rm -f tlb timer

.PHONY: all run clean
