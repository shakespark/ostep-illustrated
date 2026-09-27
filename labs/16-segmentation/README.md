# 第 16 章实验：分段转换与保护位

## 做什么
- `segtrans.c`：把原书的 SEG_MASK(0x3000)/SEG_SHIFT(12)/OFFSET_MASK(0xFFF) 伪代码写成 C，
  段表取自图 16.4（Code 32K/2K、Heap 34K/3K、Stack 28K/2K 反向增长），支持栈的负偏移。
- `protect.c`：打印 /proc/self/maps 中本程序、堆、栈的区域和 r/w/x 权限，然后尝试写字符串常量和代码区。

## 怎么跑
```
make
./segtrans                 # 原书例子：100→32868、4200→34920、7KB 越界、15KB→27KB …
./segtrans 0x3C00 4200     # 自己传地址
./protect
```

## 该观察到什么、为什么
- segtrans 的结果和原书一致；注意栈段的检查是 |负偏移| ≤ size（14KB 合法，14KB−1 越界），段号 10 未使用直接异常。
- protect：代码 r-xp、只读数据 r--p、数据/堆/栈 rw-p。写只读区域和代码都触发 SIGSEGV（保护异常）——这就是图 16.5 保护位的现代版本，
  信号名仍叫 "Segmentation fault"。
