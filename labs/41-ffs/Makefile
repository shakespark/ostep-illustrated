CC = gcc
CFLAGS = -O2 -Wall -Wextra

all: ffs_sim ext4_where

ffs_sim: ffs_sim.c
	$(CC) $(CFLAGS) -o $@ $<

ext4_where: ext4_where.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -rf ffs_sim ext4_where playground

.PHONY: all clean
