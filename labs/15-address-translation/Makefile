CC     = gcc
CFLAGS = -Wall -O1 -g
PROGS  = translate segv

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

run: all
	./translate
	./translate 16K 4K 0 1K 3000 4400
	./segv

clean:
	rm -f $(PROGS)

.PHONY: all run clean
