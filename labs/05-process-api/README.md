# 第 5 章实验：fork / wait / exec / 重定向 / 管道

```bash
cd labs/05-process-api
make            # 编译 p1 p2 p3 p4 var sharedfd pipe
```

| 程序 | 原书出处 | 做什么 | 该观察到什么 |
|---|---|---|---|
| `./p1` | Figure 5.1 | `fork()` 一次 | `hello` 只打印一次；父进程拿到孩子的 PID，孩子拿到 0；父子谁先打印**不确定** |
| `./p2` | Figure 5.2 | 父进程 `wait()` | 输出顺序固定：孩子一定先打印 |
| `./p3` | Figure 5.3 | 孩子 `execvp("wc")` | 孩子变身为 `wc`，`this shouldn't print out` 永远不会出现 |
| `./p4` | Figure 5.4 | fork 后、exec 前关闭 fd 1 再 open 文件 | 屏幕上什么也没有，`wc` 的结果在 `p4.output` 里（`make redirect`） |
| `./var` | 作业题 1 | fork 前 `x=100`，父子各自修改 | 父子打印的**地址相同**，值却各改各的：每个进程有自己的地址空间副本 |
| `./sharedfd` | 作业题 2 | open 后 fork，父子都往同一个 fd 写 | 两者的行交错出现、互不覆盖：父子共享同一个"打开文件"及其偏移量 |
| `./pipe` | 作业题 8 | 两个孩子用 `pipe()`+`dup2()` 连成 `ls -1 / \| wc -l` | 输出一个数字，与直接在 shell 里敲 `ls -1 / \| wc -l` 相同 |

其他 make 目标：

- `make order`：跑 20 次 `p1`，统计 fork 之后谁先打印。在 Linux 上通常父进程先（fork 返回后父进程还在 CPU 上），
  但**偶尔会是孩子** —— 不能依赖这种顺序。`make order1` 把进程绑到一个 CPU 上再试。
- `make buffer`：**缓冲区陷阱**。`./p1 | cat` 时 `hello` 会出现**两次**！
  因为 stdout 接到管道时是全缓冲，`printf("hello")` 还在用户态缓冲区里没写出去，fork 把缓冲区连同地址空间一起复制了一份，
  父子退出时各自刷新一次。解决办法：fork 前 `fflush(stdout)`（`p3.c` 就这样做了）。

## 为什么重定向能这样实现

UNIX 分配文件描述符时总是选**最小的空闲编号**。孩子先 `close(1)`，fd 1 就空出来了，
紧接着的 `open()` 必然拿到 1。`exec()` 替换了代码、堆、栈，但**打开文件表保留不变**，
于是 `wc` 往 fd 1 里 `printf`，数据就进了文件。`wc` 本身一行代码都不用改 —— 这就是 fork 与 exec 分开的妙处。

## 示例输出（你的机器上 PID、地址会不同）

```
$ ./p1
hello (pid:345269)
parent of 345270 (pid:345269)
child (pid:345270)
$ ./p3
hello (pid:344568)
child (pid:344569)
  29  130 1176 p3.c
parent of 344569 (rc_wait:344569) (pid:344568)
$ make redirect
  32  138 1083 p4.c
$ make order
      1 child
     19 parent
$ make buffer
hello (pid:344440)
parent of 344442 (pid:344440)
hello (pid:344440)
child (pid:344442)
$ ./var
fork 前: x=100  *heap=100  &x=0x7ffe83774710  heap=0x5d82f3f722a0
child : x=101  *heap=101  &x=0x7ffe83774710  heap=0x5d82f3f722a0
parent: x=99   *heap=99   &x=0x7ffe83774710  heap=0x5d82f3f722a0
```
