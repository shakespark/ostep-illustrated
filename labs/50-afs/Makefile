CC     = gcc
CFLAGS = -Wall -Wextra -O2

all: afs_sim

afs_sim: afs_sim.c
	$(CC) $(CFLAGS) -o $@ afs_sim.c

run: afs_sim
	./afs_sim

clean:
	rm -f afs_sim

.PHONY: all run clean
