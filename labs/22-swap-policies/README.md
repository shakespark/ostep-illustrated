# 第 22 章实验：页替换策略模拟器（C 版 paging-policy）

对应页面：`chapters/22-swap-policies.html`

## 做什么

- `paging-policy.c`：仿 OSTEP 官方 `paging-policy.py` 的 C 实现，支持 OPT / FIFO / LRU / CLOCK / RAND。
  逻辑与网页模拟器一致（OPT 平局时取内存列表中靠后的页，与官方工具相同），可用来交叉验证原书 Figure 22.1–22.5 与 Belady 异常。
- `workload.c`：复刻 Figure 22.6–22.8 的三种工作负载（无局部性、80-20、循环顺序），打印各策略在不同缓存大小下的命中率。

## 怎么跑

```sh
make
./paging-policy -p ALL -c 3 -a 0,1,2,0,1,3,0,3,1,2,1        # 原书序列，四种策略对比
./paging-policy -p OPT -c 3 -a 0,1,2,0,1,3,0,3,1,2,1 -v     # 逐步追踪（-v）
./paging-policy -p FIFO -c 3 -a 1,2,3,4,1,2,5,1,2,3,4,5     # Belady：3 次命中
./paging-policy -p FIFO -c 4 -a 1,2,3,4,1,2,5,1,2,3,4,5     # Belady：只有 2 次命中！
./paging-policy -p LRU -c 3 -n 10 -m 10 -s 1 -v             # 随机生成 10 个访问（Homework 1）
./workload                                                  # 三种负载的命中率表
```

## 该观察到什么

| 策略 | 原书序列命中 | 命中率 | 不计冷启动 |
|------|------|------|------|
| OPT  | 6 | 54.5% | 85.7% |
| FIFO | 4 | 36.4% | 57.1% |
| LRU  | 6 | 54.5% | 85.7% |

- FIFO 在 Belady 序列上：缓存 3 页命中 3 次，缓存 4 页只命中 2 次（Belady 异常）。
- `workload` 输出里：无局部性时 LRU/FIFO/RAND 几乎相同；80-20 时 LRU > CLOCK > FIFO≈RAND；
  循环顺序时 LRU/FIFO/CLOCK 在缓存 49 页时命中率为 0%，RAND 却很高。

## 为什么

- OPT 看未来，是下界标尺；LRU 利用局部性，在这个小序列上"恰好"与 OPT 持平。
- FIFO 没有栈特性（大缓存内容不一定包含小缓存内容），所以会出现 Belady 异常。
- 循环访问时，"最老的页"正是马上要用的页，LRU/FIFO 每次都踢错；Random 没有这种系统性偏差。

## 进阶（Homework 5）

用 valgrind 获取真实程序的访问序列，转换成页号后喂给模拟器：

```sh
valgrind --tool=lackey --trace-mem=yes ls 2> raw.txt
awk '/^ [LSM]|^I/ { split($2, a, ","); printf "%d\n", strtonum("0x" a[1]) / 4096 }' raw.txt > trace.txt
./paging-policy -p LRU -c 64 -f trace.txt
```

逐渐增大 `-c`，画出命中率随缓存大小变化的曲线，看看 `ls` 的工作集有多大。
（`strtonum` 需要 GNU awk；本仓库只保证 `make` 与上面的基本命令可直接运行。）
