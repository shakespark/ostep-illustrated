# 第 43 章实验：日志结构文件系统（LFS）

本目录有两个小实验：

| 文件 | 做什么 |
| --- | --- |
| `seqbuf.c` | 在真实磁盘上测“写缓冲”的效果：随机 4KB 写、顺序但逐块落盘、攒成段再顺序写（段大小 16KB～8MB），每次落盘都调用 `fdatasync()` |
| `lfs_mini.py` | 一个迷你 LFS 模拟器（思路仿 ostep-homework 的 `lfs.py`）：CR、imap 片段、inode、目录块、段摘要块 SS，支持创建/写/删除/硬链接，以及按书中伪代码判断存活并清理段 |

## 怎么跑

```bash
cd labs/43-lfs
make            # 编译 seqbuf
./seqbuf        # 约 5～10 秒，会在当前目录建一个 64MB 的临时文件，结束后删掉
python3 lfs_mini.py -L "c,/foo:w,/foo,0,1" -c
python3 lfs_mini.py -L "c,/foo:w,/foo,0,1:w,/foo,1,1:w,/foo,2,1:w,/foo,3,1" -c
python3 lfs_mini.py -L "c,/foo:w,/foo,0,4" -c
python3 lfs_mini.py --demo-clean
make run        # 一次跑完上面三类
```

`lfs_mini.py` 的命令格式（与 `lfs.py` 的 `-L` 相同）：
`c,/路径` 创建文件，`d,/路径` 创建目录，`w,/路径,起始块,块数` 写文件，`r,/路径` 删除，`l,/已有,/新名` 建硬链接。
`-c` 在每个块前标出 `live`（从 CR 出发可达）。

## 该观察到什么

### seqbuf

- 每次 `fdatasync()` 都有一笔近似固定的开销（本机约 1ms，相当于原书里的 T<sub>position</sub>）。
  只写 4KB 就落盘，带宽只有几 MB/s——**不管是随机写还是顺序写**。这正是原书 43.2 节说的：
  “仅仅按顺序写还不够，必须一次写很多”。
- 段越大，固定开销被摊得越薄，带宽一路上涨，最后接近设备的峰值（本机几百 MB/s）后趋于平缓。
  用原书式 (43.6) 可以反推：若 T<sub>position</sub>≈1ms、R<sub>peak</sub>≈500MB/s，想拿到 90% 峰值需要
  D = 0.9/0.1 × 500MB/s × 0.001s ≈ 4.5MB，和实测“4MB 段附近开始饱和”吻合。
- 在 WSL2 的虚拟磁盘（底层通常是 SSD）上，随机与顺序的 4KB 写差别不大；在机械硬盘上随机写会慢一个数量级。
  但“攒大块再写”在两种设备上都有效——这也是原书总结里说 LFS 风格在 SSD 上同样有意义的原因。

### lfs_mini.py

- `c,/foo:w,/foo,0,1`：创建文件写出 I[1]、新目录块、新根目录 inode、imap 片段；写一个块写出 **数据块 D → inode I → imap 片段**，
  然后 CR（地址 0）指向最新的 imap 片段。
- 四次单块写 vs 一次写四块：最终存活块数一样多（9 个），但前者多写了 6 个块的垃圾（旧 inode、旧 imap 片段）——
  这就是为什么真实 LFS 要在内存里缓冲更新。
- `--demo-clean`：第一轮直接回收“全死”的段；第二轮清理利用率最低的两个段，逐块打印 `(N, T) = SS[A]` 判断存活，
  搬走活块后还要重写相关 inode 和 imap 片段，于是**其他段里的旧 inode 也变成了垃圾**（S5 从 5/7 降到 2/7）。
  清理前后文件内容（每块的版本号）完全相同。

## 示例输出（你的机器上数字会不同）

```
mode                   write       total   syncs   time(s)      MB/s   ms/sync
1 rand-inplace          4 KB     2000 KB     500     0.485      4.03     0.969
2 seq-sync-each         4 KB     2000 KB     500     0.420      4.65     0.840
3 seg-buffered         16 KB    65536 KB    4096     4.945     12.94     1.207
3 seg-buffered         64 KB    65536 KB    1024     1.121     57.12     1.094
3 seg-buffered        256 KB    65536 KB     256     0.436    146.72     1.704
3 seg-buffered       1024 KB    65536 KB      64     0.301    212.95     4.696
3 seg-buffered       4096 KB    65536 KB      16     0.138    463.15     8.637
3 seg-buffered       8192 KB    65536 KB       8     0.145    442.38    18.084
```
