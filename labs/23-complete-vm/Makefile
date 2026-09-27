CC = gcc
CFLAGS = -O1 -Wall -Wextra

all: cow aslr mmap_touch

cow: cow.c
	$(CC) $(CFLAGS) -o $@ $<

# -fPIE -pie：让代码段也参与随机化（现代发行版的默认）
aslr: aslr.c
	$(CC) $(CFLAGS) -fPIE -pie -o $@ $<

mmap_touch: mmap_touch.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f cow aslr mmap_touch
