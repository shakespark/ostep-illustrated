CC = gcc
CFLAGS = -Wall -Wextra -O2

all: echo_server client pipes

echo_server: echo_server.c
	$(CC) $(CFLAGS) -o $@ $<
client: client.c
	$(CC) $(CFLAGS) -o $@ $<
pipes: pipes.c
	$(CC) $(CFLAGS) -o $@ $<

demo: all
	./run_demo.sh

clean:
	rm -f echo_server client pipes server.log
.PHONY: all demo clean
