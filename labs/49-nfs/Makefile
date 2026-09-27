CC     = gcc
CFLAGS = -Wall -Wextra -O2

all: nfs_sim

nfs_sim: nfs_sim.c
	$(CC) $(CFLAGS) -o $@ nfs_sim.c

run: nfs_sim
	./nfs_sim

clean:
	rm -f nfs_sim

.PHONY: all run clean
