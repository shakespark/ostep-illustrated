CC     = gcc
CFLAGS = -O2 -Wall -Wextra -std=gnu11
PROGS  = mycat links mystat fork-seek atomic-write

all: $(PROGS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(PROGS) foo file file2 file3 alongerfilename file.txt out.txt foo.txt foo.txt.tmp
	rm -rf d

.PHONY: all clean
