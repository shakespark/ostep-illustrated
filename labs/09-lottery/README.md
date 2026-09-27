# 第 9 章实验：nice 值真的决定 CPU 份额吗？

`nice_share.c` 创建两个子进程，把它们绑定到**同一个 CPU**（`sched_setaffinity`，CPU 0），
分别用 `setpriority` 设置 nice 值，然后两个进程都做空循环计数若干秒，
最后把循环次数报告给父进程。父进程打印各自的实际份额，以及按 CFS 权重表
（`prio_to_weight`，与原书 9.7 节相同）算出的理论份额 weight_i / (weight_1 + weight_2)。

## 编译与运行

```sh
make
./nice_share 0 5 3        # nice 0 对 nice 5，运行 3 秒：理论 75.3% : 24.7%
./nice_share 5 10 3       # nice 差值同样是 5：比例应几乎相同
./nice_share 0 0 2        # 相同 nice：各约 50%
sudo ./nice_share -5 0 3  # 负 nice 需要 root（否则 setpriority 报 Permission denied）
```

## 本机（WSL2，Linux 6.6）示例输出（你的数字会不同）

```
both children pinned to CPU 0, running 3 s
child 1: nice   0  weight  1024  loops  5841542322  share 75.5%  (expected 75.3%)
child 2: nice   5  weight   335  loops  1894193102  share 24.5%  (expected 24.7%)
```

## 该观察到什么、为什么

1. 两个 CPU 密集型进程在同一个 CPU 上竞争时，循环次数之比非常接近权重之比 1024 : 335 ≈ 3 : 1。
   CFS 让每个进程的 vruntime 以 1024/weight 的速率增长，总是运行 vruntime 最小者，
   所以长期看各进程获得的 CPU 时间与权重成正比。
2. `5 10` 与 `0 5` 的比例几乎一样：权重表相邻两项之比约为 1.25，所以比例只取决于 nice 差值。
3. 如果不绑定 CPU（去掉 `sched_setaffinity`），两个进程会被放到不同的 CPU 上各自独占，
   nice 值通常就几乎没有影响——比例份额只在"争同一个 CPU"时才体现出来（多 CPU 调度见第 10 章）。
4. Linux 6.6 起默认调度器从 CFS 换成了 EEVDF（原书 9.8 节提到），但 nice→权重表不变，
   按权重分配 CPU 的比例行为也不变。
