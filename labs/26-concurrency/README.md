# 第 26 章实验：亲眼看到竞态条件

对应页面：`chapters/26-concurrency.html`

## 文件
| 文件 | 内容 |
|---|---|
| `t0.c` | 原书 Figure 26.2：创建两个线程分别打印 A、B，主线程 join 等它们结束 |
| `t1.c` | 原书 Figure 26.6：两个线程各对共享变量 `counter` 加 1000 万次（可用参数改次数） |
| `t1_lock.c` | t1 的修正版：用 `pthread_mutex_t` 保护 `counter = counter + 1`（预告第 27/28 章） |
| `common_threads.h` | 把 `pthread_create/join/mutex_lock/unlock` 包装成出错即退出的版本（同原书） |

## 怎么跑
```bash
cd labs/26-concurrency
make            # gcc -Wall -O0 -g -pthread
./t0            # main: begin / A / B / main: end（A、B 顺序理论上不确定）
./t1            # 期望 20000000，实际几乎每次都更小、而且每次不同
make race       # 连续跑 5 次 t1，只看最后一行
./t1 1000       # 次数很少时往往"碰巧正确"：线程 A 在 B 启动前就跑完了
make single     # 用 taskset -c 0 把进程钉在一个 CPU 上，看单处理器上的竞态
make disasm     # objdump 看 counter = counter + 1 编译成的三条指令
./t1_lock       # 加锁后永远是 20000000
```

## 该观察到什么
1. `t1` 的结果几乎每次都不是 20000000，且每次都不一样——这就是**不确定性（indeterminate）**。
2. `make disasm` 能看到与原书几乎一样的三条指令（x86-64，地址不同）：
   ```
   mov    0x2e91(%rip),%eax     # 读 counter 到 eax
   add    $0x1,%eax             # eax + 1
   mov    %eax,0x2e88(%rip)     # 写回 counter
   ```
   两条 mov 之间任何时刻都可能发生中断或另一个 CPU 的写入。
3. 多核上丢失非常多（本机常见约 1050 万，即丢了将近一半）：两个线程真正同时在两颗 CPU 上跑，几乎每次读-改-写都可能撞车。
4. 单 CPU（`make single`）上有时完全正确，有时丢掉几百万：只有定时器中断恰好落在"读"和"写"之间才会出错；
   可一旦出错，被打断的线程恢复后会把一个**很旧的值 + 1** 写回去，抹掉另一个线程整整一个时间片里的所有累加。
5. `t1_lock` 总是正确，但更慢：互斥带来了正确性，也带来了开销（第 28、29 章详细讨论）。

## 为什么用 -O0
开了 `-O2` 后编译器可能把循环整个优化掉或把 `counter` 放进寄存器（尽管有 `volatile`，指令形态也会变），
为了让反汇编和原书的"读-加-写"三条指令一一对应，这里用 `-O0`。
