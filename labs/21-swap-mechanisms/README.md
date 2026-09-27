# 第 21 章实验：present 位与按需分页

对应页面：`chapters/21-swap-mechanisms.html`

## 做什么

- `present.c`：通过 `/proc/self/pagemap` 在用户态读出每个虚拟页的页表状态
  （bit 63 = present，页在内存；bit 62 = swapped，页在交换空间），
  再用 `madvise(MADV_PAGEOUT)`（Linux 5.4+）请内核把页换出，最后重新访问把页换回。
- `mem.c`：仿原书 Homework 的 `mem.c`，分配 N MB 并反复逐个触碰，
  报告每一轮的耗时、带宽，以及用 `getrusage` 统计的 minor/major page fault 次数。

## 怎么跑

```sh
make
./present
./mem 1024 3          # 另开一个终端运行 `vmstat 1`，观察 swpd/free/si/so
```

## 该观察到什么（本机 WSL2 实测，有 4 GB 交换空间）

```
after mmap:                      ................   present= 0 swapped= 0
after writing even pages:        P.P.P.P.P.P.P.P.   present= 8 swapped= 0
after MADV_PAGEOUT:              S.S.S.S.S.S.S.S.   present= 0 swapped= 8
after reading even pages:        P.P.P.P.P.P.P.P.   present= 8 swapped= 0
data intact after swap-in? yes
```

```
loop 0:  1113.53 ms      919.6 MB/s  minor faults  262145  major faults 0
loop 1:    92.97 ms    11013.8 MB/s  minor faults       0  major faults 0
```

## 为什么

- `mmap`/`malloc` 只分配虚拟地址空间；页第一次被触碰时产生页错误，OS 才分配页框（并清零）。
  所以 `mem` 的第 0 轮有约 `MB × 256` 次 minor fault，之后的轮次没有页错误，快一个数量级。
- 换出后，PTE 的 present 位为 0，页表项里改记页在交换空间的位置（`S`）。再次访问触发页错误，
  OS 把页读回并把 present 置 1。这里记成 *minor* fault，是因为刚换出的页还留在内核的
  交换缓存（swap cache）里，没有真正读磁盘；只有需要磁盘 I/O 的才记为 major fault。
- 没有交换空间（`swapon -s` 为空）时，`MADV_PAGEOUT` 无法换出匿名页，`present.c` 第三行会仍显示 `P`。
- 想看真正的换页：让 `./mem` 的分配量接近或超过 `free -m` 的 available（机器会变卡，谨慎），
  观察 `vmstat` 中 si/so 变为非零、带宽跌到存储设备的水平（原书 Homework 3、5）。
