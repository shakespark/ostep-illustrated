# 第 4 章实验：亲眼看看"进程"

原书第 4 章的官方作业是模拟器 `process-run.py`（本站第 4 章页面里有它的网页版）。
这里补充三个在真实 Linux 上运行的小程序，把书里的抽象概念对应到真实系统。

```bash
cd labs/04-process
make          # 编译 states layout pcb
make run      # 依次运行三个程序
```

## 1. `./states` —— 进程状态

父进程 fork 出 4 个子进程，让它们分别：忙着计算、睡眠、被 `SIGSTOP` 暂停、立即退出。
然后调用 `ps -o pid,ppid,stat,cmd --ppid <父PID>` 看 `STAT` 列：

| STAT | Linux 含义 | 对应原书的状态 |
|---|---|---|
| `R` | running 或 runnable | Running / Ready（Linux 不区分"正在跑"和"就绪"，都记为 R） |
| `S` | 可中断睡眠，等待某个事件 | Blocked |
| `T` | 被信号暂停 | 原书 4.2 节 "Miscellaneous Control" 里的 suspend |
| `Z` | 已退出、父进程还没 `wait()` | 原书 4.5 节的 zombie（最终状态） |

之后父进程 `SIGCONT` 恢复被暂停的孩子，再逐个 `waitpid()` 回收 —— 僵尸就消失了。

## 2. `./layout` —— 地址空间与加载

打印 `main` 函数（代码）、全局变量（静态数据）、`malloc` 的结果（堆）、局部变量（栈）、`argv[0]`（OS 在栈上准备的参数）的地址。
你会看到：代码和静态数据挨在一起（都从可执行文件加载），堆在它们上方不远，栈在很高的地址。

程序还有一个 64 MiB 的未初始化全局数组。**访问之前** `VmRSS`（实际占用的物理内存）只有 1 MiB 多；
**写满之后**才涨到约 65 MiB —— 这就是原书说的现代操作系统"懒加载（lazy）"：用到哪一页才准备哪一页。

## 3. `./pcb` —— Linux 里的 PCB

原书 Figure 4.5 是 xv6 的 `struct proc`；Linux 里对应的结构叫 `task_struct`，其中一部分可以通过 `/proc/<pid>/` 读到。
程序打开一个文件后 fork，父子各自打印：状态、PID、父 PID、上下文切换次数、打开文件表。
注意子进程的 `PPid` 就是父进程的 `Pid`，而且子进程**继承了父进程打开的 fd 3**（第 5 章会用到这一点）。

## 示例输出（你的机器上 PID、地址会不同）

```
$ ./states
$ ps -o pid,ppid,stat,cmd --ppid 343636
    PID    PPID STAT CMD
 343637  343636 R    ./states
 343638  343636 S    ./states
 343639  343636 T    ./states
 343640  343636 Z    [states] <defunct>
waitpid 回收了 343637，退出码 0
...
$ ./layout
代码   main()        @ 0x569bf8872273
静态   initialized   @ 0x569bf8875060
静态   big[] (.bss)  @ 0x569bf88750a0
堆     malloc()      @ 0x569c3333d2a0
栈     local         @ 0x7ffe2464fe44
懒加载（lazy）：
  访问 big[] 之前 VmRSS =   1408 KiB
  写满 big[] 之后 VmRSS =  67072 KiB
```
