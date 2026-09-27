# 第 32 章实验：亲手制造（和消灭）一次死锁

对应页面：`chapters/32-concurrency-bugs.html`

## 文件

| 文件 | 做什么 |
|---|---|
| `vector_deadlock.c` | 两个线程反复做 `vector_add`：线程 0 做 `add(v0, v1)`，线程 1 做 `add(v1, v0)`。每次都要持有两把锁。四种模式对应原书的四种写法。 |
| `atomicity.c` | 原书 Figure 32.2 的 MySQL `proc_info` 原子性违反：线程 1 "先检查、再使用"，线程 2 在中间把指针置 NULL。 |

## 怎么跑

```bash
cd labs/32-concurrency-bugs
make                         # gcc -Wall -Wextra -O2 -pthread
./vector_deadlock naive      # 先锁 dst 再锁 src，两线程顺序相反 → 很快死锁
./vector_deadlock ordered 1000000   # 按锁地址排序 → 破坏"循环等待"
./vector_deadlock trylock 1000000   # lock + trylock，失败全部释放重来 → 绕开"非抢占"
./vector_deadlock global  1000000   # 先拿全局 prevention 锁 → 破坏"持有并等待"
./atomicity bug              # 统计"检查时非 NULL、使用时却是 NULL"的次数
./atomicity fixed            # 加锁后应为 0
make run                     # 一次跑完上面全部
```

`naive` 模式不会永远挂住：主线程是个看门狗，1 秒内 `vector_add` 计数没有任何增长就打印
两个线程各自"持有谁、等待谁"，然后 `exit(2)` 结束整个进程。

## 该观察到什么

1. **naive 经常死锁**（在我们的机器上默认参数跑 20 次，有 10 次死锁），死锁时往往只完成了几次到几千次 `vector_add`。报告里能看到
   `T0 持有 v0 等 v1`、`T1 持有 v1 等 v0`——这就是原书 Figure 32.7 的依赖图里的环。
   把次数调小（`./vector_deadlock naive 1000`）多跑几次，会发现有时能侥幸跑完：
   死锁是"可能"发生，而不是"必然"发生，这正是它难以测试的原因。
2. **ordered / global 永远不会死锁**。它们分别破坏了四个必要条件中的"循环等待"和"持有并等待"。
3. **trylock 也不会死锁，但重试次数可能非常大**（甚至超过实际完成次数）：两个线程不断地
   "拿到第一把、抢不到第二把、全部放掉重来"，这就是原书说的**活锁（livelock）**的苗头。
   它们都在跑，只是很多工作白做了。
4. **atomicity bug 会出现几千次违反**；真实的 MySQL 代码在这里会 `fputs(NULL)` 直接崩溃。
   加上 `proc_info_lock` 之后（`fixed`）违反次数恒为 0，代价是线程 1 和线程 2 互相等锁、变慢。

## 为什么

- 死锁需要四个条件同时成立：互斥、持有并等待、非抢占、循环等待。`naive` 四个全占；
  其余三种模式各拆掉一个，所以都安全。
- 原子性违反的本质是"一段本应原子执行的代码（check + use）在执行时没有被保护"，修复就是把它们放进同一个临界区。
- 你机器上的数字（完成次数、用时、重试次数、违反次数）会和页面上的示例不同，而且每次运行都不同。
