# OSTEP 图解学习站：章节页编写规范

读者：中文母语的学习者，正在读《Operating Systems: Three Easy Pieces》。他们觉得光读书效率低，
这个网站的目标是让他们通过**交互模拟器 + 大量图解 + 先预测再验证**更快、更深地理解每一章。

## 目录结构
```
/home/lee/operating_systems_three_easy_pieces/
  assets/style.css  assets/ostep.js  assets/chapters.js   ← 共享，只读，不要修改
  chapters/<slug>.html                                     ← 你写的章节页（slug 见 chapters.js）
  labs/<slug>/  *.c  Makefile  README.md                   ← 你写的 C 实验
  source/text/<name>.txt                                   ← 原书章节全文（pypdf 提取），写作依据
  source/pdf/<name>.pdf                                    ← 原书 PDF（图需要时可以看）
```

## 硬性要求
1. **忠实于原书**：先完整阅读 `source/text/<name>.txt` 再动笔。概念、术语、数字示例和书中 THE CRUX 以原书为准；
   可以用自己的方式重新组织和补充讲解，但不能与原书矛盾。术语写成"中文（English）"，如"上下文切换（context switch）"。
2. **每页一个独立 HTML 文件**，无构建步骤，浏览器直接打开。页面骨架：
   ```html
   <!doctype html>
   <html lang="zh-CN">
   <head>
   <meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
   <title>第 N 章 · 标题</title>
   <link rel="stylesheet" href="../assets/style.css">
   <script src="../assets/chapters.js"></script>
   <script src="../assets/ostep.js"></script>
   <style>/* 仅本页需要的少量样式；颜色一律用 var(--token) */</style>
   </head>
   <body data-chapter="N">
   <main>
     ...内容（章节头、本页目录、底部导航、"已学完"勾选都由 ostep.js 自动注入，不要自己写）...
   </main>
   <script>/* 本页模拟器代码，放在 main 之后 */</script>
   </body></html>
   ```
3. **不能依赖任何外部网络资源**（无 CDN、无外链字体、无图片外链）。所有图用内联 SVG 或 DOM 画。
4. **主题**：颜色只能用 `style.css` 里的 token（`--fg --muted --surface --surface-2 --border --accent --accent-soft --ok --bad --warn` 及其 `-soft`、实体色 `--c1..--c8`）。
   不要写死 `#fff`/`#000` 背景或文字（例外：在 `--c1..--c8` 实色块上写白字）。SVG 文字默认已是 `var(--fg)`。
   深浅两种主题都要清晰。
5. **窄屏可用**（约 400px 宽）：宽的 SVG/表格放在 `figure.fig` / `.table-wrap` / `.stage` 里（它们会横向滚动），正文不能横向溢出。
6. 写完后**自检**：用 `node` 或 `python3` 做基本检查（HTML 标签闭合、JS 语法：可以把 `<script>` 内容提取出来 `node --check`），
   并把模拟器的核心算法逻辑单独抽出来用 node 跑几组数据，确认数值与原书示例一致（例如调度周转时间、TLB 命中率、页表地址转换结果）。

## 每章的内容结构（按顺序，用 `<h2>` 分节，数量可按章节调整）
1. **一句话 + 关键问题**：`<div class="crux">` 写本章 THE CRUX（中文，附原文英文一句）；再用 2-4 句话说明本章要解决什么问题、它在全书中的位置（承上启下）。
2. **核心概念速览**：`.cards` 网格，4-8 张卡片，每张一个关键概念 + 一句话解释。
3. **图解讲解**：本章主体。按原书逻辑逐节讲清楚，**大量使用内联 SVG 示意图**（`<figure class="fig">…<figcaption>`）。
   每个重要机制至少一张图。讲解要比原书更直观，但不要注水：读者应能只看这一页就掌握本章要点。
   重要代码片段（书中的 C 代码、伪代码）用 `<pre><code>` 展示并逐行解释关键点。
4. **交互模拟器（重点，每章至少 1 个，核心章节 2-3 个）**：用 `.sim` 组件：
   ```html
   <section class="sim" id="sim-xxx">
     <div class="sim-head"><span class="tag">模拟器</span><h3>标题</h3><div class="desc">能玩什么、该观察什么</div></div>
     <div class="sim-body">
       <div class="controls">…参数输入、按钮…</div>
       <div class="stage">…可视化…</div>
       <div class="narrate">…当前这一步发生了什么（中文解说）…</div>
       <div class="stats">…<div class="stat"><div class="v">12.5</div><div class="l">平均周转时间</div></div>…</div>
     </div>
   </section>
   ```
   - 要能**改参数**（作业长度、页大小、地址、策略、随机种子等）并**单步执行**：用 `OSTEP.stepper(host, {steps, render})`，每一步有中文解说（`.narrate`）。
   - 提供"载入原书示例"的预设按钮，能复现书中的图/例子。
   - 模拟器应该揭示机制本身（比如页表遍历的每一步、线程交错的每条指令），而不是只给最终数字。
   - 可以参考官方 homework 模拟器的思路（如 scheduler.py、paging-linear-translate.py、x86.py、vsfs.py），在网页里重现。
5. **先预测再验证**：3-6 道 `.exercise`，题目引导读者先在模拟器里预测再验证，答案放 `<details class="reveal"><summary>看答案</summary><div>…</div></details>`。
   多改编自原书章末 Homework 题。
6. **动手实验（C 语言，适用时）**：在 `labs/<slug>/` 放可编译的小程序 + `Makefile` + `README.md`（中文说明：做什么、怎么跑、该观察到什么、为什么）。
   页面里用 `<div class="lab">` 介绍并给出命令（`cd labs/<slug> && make && ./xxx`）。**必须在本机实际编译运行过**（Linux/WSL2，gcc 14），
   把真实输出片段贴进页面（注明"示例输出，你的机器上数字会不同"）。纯概念章节（如分布式/NFS）可用小型 C 或 Python 模拟代替。
7. **自测题**：6-10 道选择题 `.mcq`，覆盖易错点，每题有解释。开头放 `<p class="quiz-score"></p>`。
   ```html
   <div class="mcq" data-answer="1"><div class="q">题干</div>
     <div class="opts"><button>A. …</button><button>B. …</button><button>C. …</button></div>
     <div class="explain">为什么选 B…</div></div>
   ```
   `data-answer` 是正确选项下标（0 基）。**正确答案的位置要分散**，不要总是同一个。
8. **常见误区**（`.warn`）和**本章小结**（要点列表 + 与后续章节的联系）。

可用的其他组件：`.tip`、`.aside`（`data-title="…"`）、`.key`、`.cell`（`.on/.hit/.miss`）、`.pill`、`.readout`、表格 `.table-wrap > table`。

## `window.OSTEP` 工具函数（assets/ostep.js）
- `OSTEP.h(tag, attrs, ...children)` 创建 DOM 元素；`OSTEP.s(tag, attrs, ...children)` 创建 SVG 元素（`attrs.text` 设置文字）。
- `OSTEP.color(i)` → `var(--c{i+1})`；`OSTEP.letter(i)` → 'A','B',…
- `OSTEP.rng(seed)` 可复现随机数，带 `.int(lo,hi)`、`.pick(arr)`。
- `OSTEP.hex(n, width)`、`OSTEP.bin(n, width)` 数字格式化。
- `OSTEP.stepper(host, {steps: N | () => N, render(i), autoplayMs})` 单步控件（⏮ ◀ ▶ ⏵），返回 `{go, reset, refresh, stop, index}`；参数变化后调用 `refresh()` 或 `reset()`。
- `OSTEP.gantt(host, [{who, start, end, label?}], {unit, names, pxPer})` 画时间线（who=-1 为空闲）。
- `OSTEP.wireMcq(root)`：动态插入选择题后调用。

## 质量标准
- 不要写"占位""TODO""此处略"。每个模拟器都必须真的能运行。
- 中文自然、准确、不啰嗦；解释"为什么"而不只是"是什么"。
- 一章页面的体量大约相当于把原书这一章讲透：通常 800-1500 行 HTML（含脚本）是合理的。
