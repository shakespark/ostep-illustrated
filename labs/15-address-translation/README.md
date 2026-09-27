# 第 15 章实验：基址 + 界限转换与越界异常

## 做什么
- `translate.c`：用 C 实现 MMU 的"先查界限、再加基址"，复现原书两组例子
  （x = x + 3：base 32KB → 128→32896、15KB→48128；4KB 地址空间 @16KB：0→16384、1KB→17408、3000→19384、4400→越界）。
- `segv.c`：在真实进程里访问界外地址（mprotect 成 PROT_NONE 的页、地址 16、内核地址），捕获 SIGSEGV，打印硬件报告的出错地址。

## 怎么跑
```
make
./translate
./translate 16K 4K 0 1K 3000 4400     # 自定义：<base> <bounds> <va>...
./segv
```

## 该观察到什么、为什么
- translate 的输出与原书数字完全一致：PA = VA + base，合法条件 0 ≤ VA < bounds。
- segv：最后一个合法字节可以读，越过 1 字节立刻 SIGSEGV。越界访问 → 硬件异常、切到内核态 → OS 处理程序 → 默认向进程发 SIGSEGV 终止它，
  这和原书图 15.6 最后几行（B 执行坏的 load）是同一个流程（现代 Linux 用分页实现，但机制思想一致）。
