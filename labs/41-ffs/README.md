# 第 41 章实验：FFS 的放置策略

两个小程序：

| 程序 | 做什么 |
| --- | --- |
| `ffs_sim` | 纯模拟。用和网页模拟器**完全相同**的算法，比较三种 inode 放置策略（ffs / spread / random）在原书示例和一个多目录工作负载上的布局、dirspan、filespan；打印大文件例外的布局；算出 chunk 分摊表（409.6 KB / 3.6 MB / 39.6 MB）。 |
| `ext4_where` | 真机观察。在你当前的 ext4 文件系统上创建 3 个目录，每个目录写 3 个 8 KB 小文件和 1 个大文件，`fsync` 后用 `FIEMAP` ioctl 查出每个文件的数据落在哪个物理块、哪个块组（block group），并用 inode 号算出 inode 属于哪个块组。 |

## 怎么跑

```bash
cd labs/41-ffs
make
./ffs_sim
./ext4_where                 # 默认在 ./playground 里建文件，大文件 64 MB，跑完自动删除
./ext4_where playground 256  # 大文件改成 256 MB
./ext4_where playground 64 keep   # 保留文件，自己用 filefrag -v / stat 再看
make clean
```

`ext4_where` 需要工作目录在 ext4 上（WSL2 的 Linux 根文件系统就是 ext4；**不要**放到 `/mnt/c` 下，那是 Windows 的 NTFS）。
它通过 `/proc/fs/ext4/<设备>/mb_groups` 数出块组个数，再用 `statvfs` 的 inode 总数除以组数得到"每组 inode 数"，不需要 root。

## 该观察到什么

`ffs_sim`：

1. 示例 1 的 `ffs` 布局与原书完全一致：`/a/c /a/d /a/e` 与 `/a` 同在 1 号组，`/b/f` 与 `/b` 同在 2 号组；
   `spread` 布局也与原书第二张图一致（c、d、e、f 各占一组）。`/a` 的 dirspan 从 16 变成 91。
2. 示例 2：关闭大文件例外时，30 块的 `/a` 几乎占满 0 号组；`L=5` 时它被切成 6 段分到 0~5 号组。
3. 示例 3：L 很小时 filespan 巨大（文件被切得太碎，跨了很多组），L 越大 filespan 越小——这正是"chunk 要足够大才能摊销寻道"的另一种体现。
4. 示例 4：多目录负载下，ffs 的平均 dirspan 只有 spread / random 的约十分之一，95% 的文件和父目录在同一组；
   代价是平均 filespan 略大（同一组里挤了更多文件，inode 和数据的距离变远了一点）。

`ext4_where`（真实系统，结果每次都会不同）：

- 同一目录下的文件拿到**几乎连续的 inode 号**，落在**同一个 inode 块组**里——FFS"同目录文件放一起"的思想还在。
  注意测试目录不是根目录的直接子目录，ext4 的 Orlov 分配器只会把"顶层目录"分散到不同组，更深的子目录会留在父目录附近，所以 a、b、c 的 inode 都在同一组。
- 小文件的数据块**彼此紧挨着**（首块号每次只差 2~3 块，一个 8 KB 文件占 2 块），这是 ext4 的"局部性组预分配"（locality group preallocation）把小文件打包在一起。
- 大文件只由**很少的几个大区段（extent）**组成——每个区段最多 32768 块（128 MB），这是在追求"每次寻道之后传输尽量多的数据"，也就是本章讲的分摊。
- 数据块所在的组和 inode 所在的组并不相同：ext4 默认启用 flex_bg（把 16 个组的元数据集中存放），而且 WSL2 的"磁盘"本身是一个虚拟磁盘文件，块组早已不对应真实的柱面。
  现代文件系统保留了"块组"这个组织方式，但具体策略比 FFS 复杂得多。
