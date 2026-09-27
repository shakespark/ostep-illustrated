CC = gcc
CFLAGS = -O1 -Wall -Wextra

all: mem present

mem: mem.c
	$(CC) $(CFLAGS) -o $@ $<

present: present.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f mem present
