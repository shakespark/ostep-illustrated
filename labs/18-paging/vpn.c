// vpn.c —— 把真实进程里的虚拟地址拆成 VPN 和 offset，并用 /proc/self/pagemap 观察"存在位"
// 编译运行：make && ./vpn
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

int g = 42;                       // 全局变量：在数据段

static void show(const char *name, const void *p, long pagesize, int shift) {
    uintptr_t a = (uintptr_t)p;
    // 中文在终端里宽度不一，名字放在最后一列，前面的数字列才能对齐
    printf("0x%-14lx  0x%-12lx  0x%03lx   %s\n", (unsigned long)a,
           (unsigned long)(a >> shift), (unsigned long)(a & (pagesize - 1)), name);
}

// 读 pagemap：每个虚拟页对应 8 字节；bit 63 = 页在内存中（present），bit 0-54 = PFN（非 root 读到 0）
static int count_present(int fd, const char *base, long npages, long pagesize) {
    int n = 0;
    for (long i = 0; i < npages; i++) {
        uint64_t entry = 0;
        off_t off = (off_t)(((uintptr_t)base / pagesize) + i) * sizeof(entry);
        if (pread(fd, &entry, sizeof(entry), off) != sizeof(entry)) return -1;
        if (entry >> 63) n++;
    }
    return n;
}

int main(void) {
    long pagesize = sysconf(_SC_PAGESIZE);
    int shift = 0;
    while ((1L << shift) < pagesize) shift++;
    int local = 7;                // 局部变量：在栈上
    int *heap = malloc(100);      // 堆

    printf("页大小 %ld 字节 → 偏移量占低 %d 位，VPN = 地址 >> %d\n\n", pagesize, shift, shift);
    printf("%-16s  %-14s  %-6s  %s\n", "virtual address", "VPN (hex)", "offset", "对象");
    show("main 函数(代码)", (void *)main, pagesize, shift);
    show("全局变量 g", &g, pagesize, shift);
    show("堆(malloc)", heap, pagesize, shift);
    show("栈(局部变量)", &local, pagesize, shift);
    show("同一页的 heap+1", heap + 1, pagesize, shift);

    // 新映射 16 页匿名内存：写之前 OS 还没给它分配物理页帧
    long npages = 16;
    char *buf = mmap(NULL, npages * pagesize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (buf == MAP_FAILED) { perror("mmap"); return 1; }
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) { perror("open /proc/self/pagemap"); return 1; }
    printf("\n一块 16 页的新内存（mmap 得到，起始 VPN 0x%lx）：\n", (unsigned long)((uintptr_t)buf >> shift));
    printf("  写入前：16 页中有 %d 页\"存在\"（pagemap bit 63）\n", count_present(fd, buf, npages, pagesize));
    for (long i = 0; i < npages; i++) buf[i * pagesize] = 1;   // 每页写一个字节
    printf("  写入后：16 页中有 %d 页\"存在\"\n", count_present(fd, buf, npages, pagesize));
    uint64_t e = 0;
    pread(fd, &e, sizeof(e), (off_t)((uintptr_t)buf / pagesize) * sizeof(e));
    printf("  第 0 页的 pagemap 记录 = 0x%016llx，PFN 字段 = %llu%s\n", (unsigned long long)e,
           (unsigned long long)(e & ((1ULL << 55) - 1)),
           (e & ((1ULL << 55) - 1)) ? "" : "（非 root 用户读到的 PFN 被内核隐藏为 0，出于安全考虑）");
    close(fd);
    munmap(buf, npages * pagesize);
    free(heap);
    return 0;
}
