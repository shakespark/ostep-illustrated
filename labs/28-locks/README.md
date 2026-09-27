# 第 28 章实验：自己动手造锁，并和 pthread_mutex 比一比

对应页面：`chapters/28-locks.html`

## 做什么
`locks.c` 用 GCC 原子内建（`__sync_*` / `__atomic_*`）实现原书里的几种锁，让 N 个线程各做 `loops` 次
`lock(); counter = counter + 1; unlock();`，最后检查 `counter == N × loops`，并计时。

| 类型 | 对应原书 | 实现 |
|---|---|---|
| `none` | —— | 不加锁（对照组） |
| `flag` | Figure 28.1 | 普通 load/store 的 flag，**错误的锁** |
| `tas` | Figure 28.3 | `__sync_lock_test_and_set`（x86 上编译成 `xchg`） |
| `ttas` | homework `test-and-test-and-set.s` | 先普通读到 0 再 TAS |
| `cas` | Figure 28.4 | `__sync_val_compare_and_swap`（x86：`lock cmpxchg`） |
| `ticket` | Figure 28.7 | `__atomic_fetch_add`（x86：`lock xadd`） |
| `yield` | Figure 28.8 | TAS 抢不到就 `sched_yield()` |
| `mutex` | 28.15 | `pthread_mutex_t`（glibc：原子操作快路径 + futex 睡眠） |

所有线程先在 `pthread_barrier_t` 上集合再同时开跑，保证真的有竞争。第 4 个参数是时间上限（秒），
超时就打印已完成多少次后退出——ticket 锁在某些情况下会慢到几个小时都跑不完。

## 怎么跑
```bash
cd labs/28-locks
make              # gcc -Wall -O2 -pthread
make correct      # 正确性：none/flag 出错，其余全部 OK
make bench        # 1/2/4/8/16 线程下各种锁的耗时
make oversub      # 线程数 = 2 × CPU 数
make single       # 所有线程钉在 1 个 CPU 上（taskset -c 0）
./locks ticket 8 1000000      # 单独跑某一种
```

## 本机结果（AMD Ryzen 7 8745H，8 核 16 线程，WSL2，gcc 14.2；你的机器上数字会不同）
```
none   threads=4   counter=1070265   expected=4000000   WRONG time=0.006s
flag   threads=4   counter=1057400   expected=4000000   WRONG time=0.011s
tas    threads=4   counter=4000000   expected=4000000   OK    time=0.140s
cas    threads=4   counter=4000000   expected=4000000   OK    time=0.148s
ticket threads=4   counter=4000000   expected=4000000   OK    time=0.308s
mutex  threads=4   counter=4000000   expected=4000000   OK    time=0.075s
```
每线程 100 万次（秒）：

| 线程数 | tas | ttas | cas | ticket | yield | mutex |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.002 | 0.002 | 0.003 | 0.003 | 0.003 | 0.007 |
| 2 | 0.030 | 0.032 | 0.033 | 0.061 | 0.006 | 0.039 |
| 4 | 0.133 | 0.094 | 0.162 | 0.315 | 0.015 | 0.076 |
| 8 | 0.569 | 0.545 | 0.590 | 1.082 | 0.042 | 0.211 |
| 16 | 2.399 | 2.015 | 3.234 | 20 秒只完成 459 万 / 1600 万 | 0.107 | 0.381 |

超额订阅（32 线程 × 20 万次）：tas 1.517s，ttas 1.341s，**ticket 10 秒只完成 18964 次**，yield 0.030s，mutex 0.143s。
单 CPU（4 线程 × 500 万次）：tas 0.136s，**ticket 5 秒只完成 53 万次**，yield 0.093s，mutex 0.150s。

## 该观察到什么、为什么
1. **`flag` 和 `none` 一样错**：检查（`while (flag == 1)`）和设置（`flag = 1`）是两步，中间可能被打断或被另一个 CPU 插队。
2. **所有自旋锁在线程变多时都急剧变慢**：锁变量所在的缓存行在各个核之间来回迁移，每次 TAS/CAS 都是一次独占写。
   ttas 先只读自旋，稍好一点。
3. **ticket 锁最公平，也最"脆弱"**：它严格按先来后到交接锁，所以每次交接都要把缓存行搬到**下一个指定的线程**所在的核；
   一旦那个线程没在运行（被抢占、线程数 ≥ CPU 数、单 CPU），所有人都得等它被调度回来——每次加锁都要付出一次调度延迟。
   这就是原书说的"自旋锁在单 CPU 上很痛苦"的极端版本。
4. **yield 和 mutex 反而最快**：它们都不公平——抢不到的线程让出 CPU / 睡眠，持锁线程释放后往往自己又立刻重新拿到锁
   （缓存还是热的），一口气做很多次。吞吐量高，但公平性差：这正是性能与公平之间的取舍。
5. 单线程时 mutex 比 tas 慢一点：glibc 的 mutex 在快路径上多做了一些检查（类型、所有者等），但仍只需一次原子操作，不进内核。
