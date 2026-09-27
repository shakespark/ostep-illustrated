# 第 14 章实验：七种常见内存错误与检测工具

## 做什么
每个程序对应一类错误（含原书作业 1、4、5、6、7）：

| 程序 | 错误 |
|---|---|
| `null.c` | 解引用 NULL（作业 1） |
| `forget_alloc.c` | 忘记分配：strcpy 到未初始化指针 |
| `too_small.c` | 分配不足：malloc(strlen(s)) 少了 '\0' |
| `overflow.c` | data[100] 越界（作业 5） |
| `uninit.c` | 读未初始化的 malloc 内存；对比 calloc |
| `leak.c` | 忘记 free（作业 4） |
| `dangling.c` | free 后继续使用（作业 6） |
| `double_free.c` | 重复释放 |
| `bad_free.c` | free 数组中间的指针（作业 7） |

Makefile 为每个程序生成两份：`xxx`（普通编译）和 `xxx-asan`（`-fsanitize=address`，含 LeakSanitizer）。
本机没有 valgrind；装了的话可以直接 `valgrind --leak-check=yes ./xxx`。

## 怎么跑
```
make
./overflow; echo "exit=$?"     # 普通版：看起来没事
./overflow-asan                # ASan：heap-buffer-overflow
make run                       # 依次运行全部 18 个版本（只显示关键行）
```

## 该观察到什么、为什么
- 普通运行时 too_small、overflow、leak、dangling 都"正常退出"（退出码 0）——能跑 ≠ 正确。
- null、forget_alloc 段错误（退出码 139 = 128 + SIGSEGV）；double_free、bad_free 被 glibc 检测到并 abort（退出码 134）。
- ASan 能精确报告越界、use-after-free、double free、invalid free 和泄漏的位置（文件:行号）。
- ASan 不报告未初始化读（它把新内存填成 0xbe），valgrind 的 memcheck 可以；`uninit` 普通版会读到上一块留下的旧数据。
- gcc 14 的 `-Wall` 能在编译期发现一部分问题（未初始化指针、use-after-free、free 非堆地址）；Makefile 里特意关掉了这些警告，好让程序编译出来演示运行时行为。
