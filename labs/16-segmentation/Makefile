CC     = gcc
CFLAGS = -Wall -O1 -g
PROGS  = segtrans protect

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./segtrans
	./protect

clean:
	rm -f $(PROGS)

.PHONY: all run clean
