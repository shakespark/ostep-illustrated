CC = gcc
CFLAGS = -O2 -Wall -Wextra

all: paging-policy workload

paging-policy: paging-policy.c
	$(CC) $(CFLAGS) -o $@ $<

workload: workload.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f paging-policy workload
