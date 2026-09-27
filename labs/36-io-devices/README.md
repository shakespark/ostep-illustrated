# 第 36 章实验：轮询 vs 阻塞等待（中断）的 CPU 代价

## 做什么
用户态程序碰不到真实设备寄存器，这里用一个"慢设备"来模拟：子进程睡眠 `delay_ms` 毫秒后往管道写 1 个字节（相当于设备完成请求）。
父进程用三种方式等它：

| 模式 | 做法 | 对应书中的概念 |
|------|------|----------------|
| `poll` | 管道设为 `O_NONBLOCK`，反复 `read()`，返回 `EAGAIN` 就再读 | 轮询（polling）：读状态寄存器，BUSY 就再读 |
| `block` | 阻塞 `read()`，进程睡眠，数据到了由内核唤醒 | 中断驱动：发请求后睡眠，完成时被唤醒 |
| `hybrid` | 先自旋 `spin_us` 微秒，没好再改成阻塞 `read()` | 两阶段（two-phased）混合 |

程序用 `getrusage()` 统计等待期间父进程消耗的 CPU 时间（用户态 + 内核态），用 `clock_gettime(CLOCK_MONOTONIC)` 统计墙钟时间，并打印轮询次数。

## 怎么跑
```bash
make                      # 编译
./wait-io poll 500        # 设备延迟 500ms，轮询等待
./wait-io block 500       # 阻塞等待
./wait-io hybrid 500 100  # 先自旋 100µs，再阻塞
make run                  # 一次跑完上面三条，外加 1ms 快设备的对比
```

## 该观察到什么
示例输出（WSL2 + gcc 14；你的机器上数字会不同）：
```
mode=poll   device=500ms  wall=  502.4 ms  cpu(user+sys)=  466.2 ms  ( 92.8% of wall)  polls=811443
mode=block  device=500ms  wall=  500.7 ms  cpu(user+sys)=    0.1 ms  (  0.0% of wall)  polls=0
mode=hybrid device=500ms  wall=  502.4 ms  cpu(user+sys)=    0.1 ms  (  0.0% of wall)  polls=180
```
- 三种方式的**墙钟时间几乎相同**：谁也不能让"设备"更快完成。
- 轮询几乎把整段等待都花在 CPU 上（八十多万次无用的 `read()`）；阻塞等待几乎不耗 CPU，这段 CPU 可以让给别的进程——这正是中断"让计算与 I/O 重叠"的价值。
- 混合方式只自旋了很短一段就睡眠了，CPU 消耗和阻塞一样低；如果设备在自旋期内就完成（把 delay 设得很小），它又能像轮询一样立刻返回。

## 为什么"快设备"那组看不出轮询更快
书上说快设备用轮询延迟更低，因为省掉了中断处理和两次上下文切换（微秒级）。但在这个实验里，"设备"本身是一个要被调度、被唤醒的进程，`fork`、`usleep` 的误差在毫秒级，远大于那点开销，所以只能清楚看到"轮询浪费 CPU"这一面。
真实系统里，这个权衡体现在内核中：Linux NVMe 驱动的 `io_poll`（高优先级 I/O 用轮询）、网卡驱动的 NAPI（高负载时关中断改轮询）都是例子。

## 可以再试试
- 把 `delay_ms` 从 1 调到 1000，看轮询的 CPU 占比如何变化。
- 同时在另一个终端跑一个计算密集程序（如 `yes > /dev/null`），用 `taskset -c 0` 把两者绑在同一个核上，比较 poll 与 block 模式下那个程序得到的 CPU。
