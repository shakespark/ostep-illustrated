# 第 23 章实验：写时复制、ASLR、按需分页与大页

对应页面：`chapters/23-complete-vm.html`

## 做什么

| 程序 | 观察内容 |
|------|----------|
| `cow.c` | 父进程写满 64 MB 后 `fork()`；子进程逐页写，统计 COW 页错误次数与耗时，并读 `/proc/self/smaps` 看 Shared_Dirty → Private_Dirty |
| `aslr.c` | 打印栈、堆、mmap 区、代码（PIE）、数据、libc 函数的地址；多次运行比较，再用 `setarch -R` 关闭 ASLR |
| `mmap_touch.c` | mmap 256 MB 后逐步写 25%/50%/100% 的页，看 RSS 与 minor fault；只读触碰（共享零页，RSS 不增长）；`MADV_HUGEPAGE` 透明大页下的页错误次数 |

## 怎么跑

```sh
make
./cow
for i in 1 2 3; do ./aslr; done
setarch -R ./aslr            # 关闭 ASLR（等价于 setarch $(uname -m) -R）
./mmap_touch
```

## 该观察到什么（本机 WSL2 实测，你的数字会不同）

```
child : fork() returned after 0.86 ms (only page tables copied, pages shared read-only)
  [child  after fork ] smaps: Rss  65536 kB   Shared_Dirty  65536 kB   Private_Dirty      0 kB
child : 1st write pass   46.14 ms, minor faults 16384   <- copy-on-write
child : 2nd write pass    0.28 ms, minor faults 0
  [child  after write] smaps: Rss  65536 kB   Shared_Dirty      0 kB   Private_Dirty  65536 kB
```

```
stack  0x7ffdad221ecc   heap  0x57848527e2a0   ...  code(main)  0x57845ff1f149
stack  0x7ffddb1dbbbc   heap  0x645f3a9c52a0   ...  code(main)  0x645f305ee149
(setarch -R) stack  0x7fffffffdc0c   heap  0x5555555592a0   ...  code(main)  0x555555555149
```

```
  wrote 100% of pages : RSS  263680 kB   minor faults so far  65538
== read-only touch of 256 MB ==
  minor faults 65536, RSS grew 0 kB, sum of bytes = 0 (all zero)
== 2MB transparent huge pages (MADV_HUGEPAGE) ==
  wrote 100% of pages : RSS  263676 kB   minor faults so far    639
```

## 为什么

- **COW**：`fork()` 只复制页表并把可写页在父子两边都标为只读；第一次写一页触发保护陷阱，
  OS 复制该页（16384 页 = 64 MB，正好每页一次）。第二遍写时页已私有，没有页错误。
- **ASLR**：每次运行各区域基址不同，但页内偏移（低 12 位）不变，同一映像内的相对距离也不变。
  所以攻击者只要泄露一个地址，往往就能推出其余地址。`/proc/sys/kernel/randomize_va_space` 为 2 表示完全开启。
- **按需分页 / 按需清零**：mmap 只建立虚拟区域；写入时每页一次页错误并分配清零的页框。
  只读访问会映射到全系统共享的"零页"，所以 RSS 不增长——又一种懒惰。
- **大页**：`/sys/kernel/mm/transparent_hugepage/enabled` 为 `madvise` 或 `always` 时，
  `MADV_HUGEPAGE` 让内核尽量用 2 MB 页，页错误次数降两个数量级（理想为 256MB/2MB = 128 次；
  未对齐的首尾部分或分配失败时会退回 4 KB 页）。每次页错误要清零 2 MB，所以总时间不一定更短，
  大页的主要收益是之后更高的 TLB 命中率。
