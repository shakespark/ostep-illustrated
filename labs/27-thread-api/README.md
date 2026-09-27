# 第 27 章实验：线程 API（创建、传参、返回值、条件变量）

对应页面：`chapters/27-thread-api.html`。所有程序都用 `gcc -Wall -pthread` 编译。

```bash
cd labs/27-thread-api
make          # 编译全部
make run      # 依次运行全部（bad_return 系列会故意报错，Makefile 里已用 || true 忽略）
make clean
```

| 文件 | 做什么 | 该观察到什么 |
|------|--------|--------------|
| `common_threads.h` | `Pthread_create` / `Pthread_join` / `Pthread_mutex_lock` 等包装宏：调用后检查返回码，非 0 就打印错误并退出 | 原书 Figure 27.4 的思路：**永远检查返回码** |
| `create_args.c` | Figure 27.1/27.2/27.3：结构体传参、堆上返回值、把 `long long` 直接塞进 `void *` 传值；再创建 3 个线程各给一份参数 | `returned 11 22`、`returned 101`；3 个线程的打印顺序每次可能不同，但 join 按顺序拿到各自结果 |
| `bad_return.c` | 原书 "oops" 反例：线程返回指向**自己栈上**局部变量的指针 | 编译时 gcc 14 已给出 `-Wdangling-pointer` 警告；运行时读到垃圾值（如 `8392704 0`），而不是 `1 2` |
| `bad_return_asan` | 同一源码加 `-fsanitize=address` 编译 | `./bad_return_asan`：线程退出后整个线程栈被回收，main 读它直接 **SEGV**；`./bad_return_asan helper`：报告 **stack-use-after-return**，并指出变量 `local` 所在的栈帧 `make_ret` |
| `cond_wait_tsan` | `cond_wait.c` 加 `-fsanitize=thread` | 只有裸 flag 那一段被报告为 data race |
| `cond_wait.c` | 用 `pthread_cond_wait` 等子线程设置 `ready`；再用裸 flag 自旋等待作对比 | 条件变量等待期间 CPU 耗时约 `0.000` 秒；自旋等待约 `0.5` 秒（空转数十亿次） |

## 为什么

* **线程的栈在线程退出时就回收了。** `pthread_join` 只把 `void *` 这个"值"交给你，
  它指向哪里由你负责。指向堆（`malloc`）或全局变量是安全的；指向线程栈上的局部变量就是悬空指针。
* 普通编译下读悬空指针是**未定义行为**：可能碰巧读到 1 2，也可能是垃圾，也可能崩溃。
  AddressSanitizer 把这种错误变成必然可见的报告。gcc 14 的 ASan 运行时默认已开启
  `detect_stack_use_after_return`，Makefile 里仍显式设置 `ASAN_OPTIONS=detect_stack_use_after_return=1`，
  以便在旧版本上也能复现。为了让报告落在 `make_ret` 这个独立栈帧上，ASan 版本用 `-O0` 编译（`-O1` 会把函数内联，
  报告类型变成 `stack-use-after-scope`）。
* `pthread_cond_wait` 让等待者**睡眠**（不占 CPU），并在睡眠时释放锁、醒来返回前重新拿到锁；
  条件用 `while` 重新检查。裸 flag 自旋既浪费 CPU，又容易写错（原书引用的研究中约一半的这类 ad hoc 同步有 bug）。

## 示例输出（本机：AMD Ryzen 7 8745H，16 个逻辑 CPU，WSL2，gcc 14.2；你的机器上地址和数字会不同）

```
== 结构体参数 + 堆上返回值 (Figure 27.2) ==
  [线程] 收到参数 a=10 b=20
  [线程] 返回堆地址 0x7761b4000b70
main: returned 11 22 (来自地址 0x7761b4000b70)
== 直接传值 (Figure 27.3) ==
  [线程] 收到值 100
main: returned 101

$ ./bad_return
main: rvals 指向 0x70b9e87f4ed0
main: returned 8392704 0 （期望 1 2）

$ ASAN_OPTIONS=detect_stack_use_after_return=1 ./bad_return_asan helper
==343743==ERROR: AddressSanitizer: stack-use-after-return on address 0x72e1d6aff024 ...
READ of size 4 at 0x72e1d6aff024 thread T1
    #0 ... in helper_thread bad_return.c:36
Address 0x72e1d6aff024 is located in stack of thread T1 at offset 36 in frame
    #0 ... in make_ret bad_return.c:18
  This frame has 1 object(s):
    [32, 40) 'local' (line 19) <== Memory access at offset 36 is inside this variable

$ ./cond_wait
== 条件变量等待 ==
  [子线程] ready = 1，signal
main: 等到了 ready（被唤醒 1 次），等待期间消耗 CPU 0.000 秒
== 反面教材：自旋等待 flag ==
main: 等到了 flag（空转 2205325635 次），等待期间消耗 CPU 0.500 秒
```

## 进一步

* 原书 Homework 使用 `valgrind --tool=helgrind` 检查数据竞争和死锁；如果你装了 valgrind，可以对
  `cond_wait` 跑一次 helgrind，它会把"裸 flag 自旋"那一段报告为数据竞争（`done_flag` 无锁读写）。
* `make cond_wait_tsan && ./cond_wait_tsan`：用 ThreadSanitizer 编译同一程序，它会报告
  `WARNING: ThreadSanitizer: data race ... Location is global 'done_flag'`——条件变量那一段则没有任何报告。
  如果 TSan 启动就报 `FATAL: ThreadSanitizer: unexpected memory mapping`（较新内核的地址随机化与 TSan 冲突，本机偶发），
  用 `setarch -R ./cond_wait_tsan` 关闭 ASLR 再运行即可。
