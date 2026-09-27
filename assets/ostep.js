// OSTEP 图解学习站：共享运行时。页面在 <body data-chapter="7"> 上声明章号，
// 本脚本自动注入顶栏、章节导航、"已学完"勾选，并接好 .mcq / pre 复制等通用组件。
// 章节页的模拟器可以使用 window.OSTEP 上的工具函数（见文件末尾导出）。
(function () {
  "use strict";
  const LS = {
    get(k, d) { try { const v = localStorage.getItem(k); return v === null ? d : JSON.parse(v); } catch (e) { return d; } },
    set(k, v) { try { localStorage.setItem(k, JSON.stringify(v)); } catch (e) {} },
  };

  // ---------- 主题 ----------
  function applyTheme(t) {
    if (t === "light" || t === "dark") document.documentElement.setAttribute("data-theme", t);
    else document.documentElement.removeAttribute("data-theme");
  }
  applyTheme(LS.get("ostep-theme", "system"));

  // ---------- DOM 小工具 ----------
  function h(tag, attrs, ...kids) {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (v == null || v === false) continue;
      if (k === "class") el.className = v;
      else if (k === "style" && typeof v === "object") Object.assign(el.style, v);
      else if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2), v);
      else if (k === "html") el.innerHTML = v;
      else el.setAttribute(k, v === true ? "" : v);
    }
    for (const kid of kids.flat()) if (kid != null && kid !== false) el.append(kid.nodeType ? kid : String(kid));
    return el;
  }
  const SVGNS = "http://www.w3.org/2000/svg";
  function s(tag, attrs, ...kids) {
    const el = document.createElementNS(SVGNS, tag);
    for (const [k, v] of Object.entries(attrs || {})) {
      if (v == null || v === false) continue;
      if (k.startsWith("on") && typeof v === "function") el.addEventListener(k.slice(2), v);
      else if (k === "text") el.textContent = v;
      else el.setAttribute(k, v);
    }
    for (const kid of kids.flat()) if (kid != null && kid !== false) el.append(kid.nodeType ? kid : document.createTextNode(String(kid)));
    return el;
  }
  // 实体配色：color(0) → var(--c1)
  const color = (i) => `var(--c${(i % 8) + 1})`;
  const letter = (i) => String.fromCharCode(65 + i);
  // 可复现随机数（mulberry32）
  function rng(seed) {
    let a = (seed >>> 0) || 1;
    const f = () => { a |= 0; a = (a + 0x6d2b79f5) | 0; let t = Math.imul(a ^ (a >>> 15), 1 | a); t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t; return ((t ^ (t >>> 14)) >>> 0) / 4294967296; };
    f.int = (lo, hi) => lo + Math.floor(f() * (hi - lo + 1));
    f.pick = (arr) => arr[Math.floor(f() * arr.length)];
    return f;
  }
  const hex = (n, w) => "0x" + (n >>> 0).toString(16).toUpperCase().padStart(w || 1, "0");
  const bin = (n, w) => (n >>> 0).toString(2).padStart(w || 1, "0");

  // ---------- 单步动画器 ----------
  // OSTEP.stepper(host, { steps: N 或 () => N, render(i), autoplayMs: 900 })
  // host 里会插入 ⏮ ◀ ▶ ⏵ 控件和"第 i / N 步"；render(i) 负责画第 i 步（0 基）。
  // 返回 { go(i), reset(), refresh(), get index() }。键盘 ←/→ 在控件获得焦点时生效。
  function stepper(host, opts) {
    const total = () => (typeof opts.steps === "function" ? opts.steps() : opts.steps);
    let i = 0, timer = null;
    const label = h("span", { class: "muted small", style: { fontVariantNumeric: "tabular-nums" } });
    const bFirst = h("button", { title: "回到开始", "aria-label": "回到开始" }, "⏮");
    const bPrev = h("button", { title: "上一步 (←)", "aria-label": "上一步" }, "◀ 上一步");
    const bNext = h("button", { class: "primary", title: "下一步 (→)", "aria-label": "下一步" }, "下一步 ▶");
    const bPlay = h("button", { title: "自动播放", "aria-label": "自动播放" }, "⏵ 播放");
    const bar = h("div", { class: "controls stepper", tabindex: "0" }, bFirst, bPrev, bNext, bPlay, label);
    host.append(bar);
    function stop() { if (timer) { clearInterval(timer); timer = null; bPlay.textContent = "⏵ 播放"; } }
    function go(k) {
      const n = total();
      i = Math.max(0, Math.min(n - 1, k));
      label.textContent = `第 ${i + 1} / ${n} 步`;
      bFirst.disabled = bPrev.disabled = i === 0;
      bNext.disabled = i >= n - 1;
      if (i >= n - 1) stop();
      opts.render(i);
    }
    bFirst.onclick = () => { stop(); go(0); };
    bPrev.onclick = () => { stop(); go(i - 1); };
    bNext.onclick = () => { stop(); go(i + 1); };
    bPlay.onclick = () => {
      if (timer) return stop();
      if (i >= total() - 1) go(0);
      bPlay.textContent = "⏸ 暂停";
      timer = setInterval(() => go(i + 1), opts.autoplayMs || 900);
    };
    bar.addEventListener("keydown", (e) => {
      if (e.key === "ArrowRight") { e.preventDefault(); stop(); go(i + 1); }
      if (e.key === "ArrowLeft") { e.preventDefault(); stop(); go(i - 1); }
    });
    const api = { go, reset: () => { stop(); go(0); }, refresh: () => go(i), stop, get index() { return i; }, bar };
    queueMicrotask(() => go(0));
    return api;
  }

  // ---------- 甘特图（调度类模拟器常用） ----------
  // OSTEP.gantt(host, segments, {unit: 10, height: 26, names:['A','B']})
  // segments: [{who: 0, start: 0, end: 10, label?}]，who 为实体下标（-1 表示空闲）
  function gantt(host, segs, o = {}) {
    const unit = o.unit || 10, rowH = o.height || 26;
    const end = Math.max(1, ...segs.map((g) => g.end));
    const pxPer = o.pxPer || Math.max(4, Math.min(28, 760 / end));
    const W = end * pxPer + 40, H = rowH + 34;
    const svg = s("svg", { viewBox: `0 0 ${W} ${H}`, width: W, height: H, role: "img", "aria-label": o.aria || "时间线" });
    for (const g of segs) {
      const x = 20 + g.start * pxPer, w = (g.end - g.start) * pxPer;
      svg.append(s("rect", { x, y: 4, width: Math.max(1, w - 1), height: rowH, rx: 3, style: g.who < 0 ? "fill:var(--surface-2);stroke:var(--border)" : `fill:${color(g.who)}` }));
      if (w > 16) svg.append(s("text", { x: x + w / 2, y: 4 + rowH / 2 + 4, "text-anchor": "middle", "font-size": 12, style: g.who < 0 ? "" : "fill:#fff;font-weight:600", text: g.label ?? (g.who < 0 ? "空闲" : (o.names ? o.names[g.who] : letter(g.who))) }));
    }
    for (let t = 0; t <= end; t += unit) {
      const x = 20 + t * pxPer;
      svg.append(s("line", { x1: x, x2: x, y1: rowH + 6, y2: rowH + 11, class: "stroke", "stroke-width": 1 }));
      svg.append(s("text", { x, y: rowH + 25, "text-anchor": "middle", "font-size": 11, class: "t-muted mono", text: t }));
    }
    host.replaceChildren(svg);
    return svg;
  }

  // ---------- 页面装配 ----------
  function chapterIndex() {
    const n = Number(document.body.dataset.chapter);
    return (window.OSTEP_CHAPTERS || []).findIndex((c) => c.n === n);
  }
  function mountChrome() {
    const list = window.OSTEP_CHAPTERS || [];
    const idx = chapterIndex();
    const inChapters = /\/chapters\//.test(location.pathname);
    const root = inChapters ? "../" : "";
    const cur = list[idx];
    const part = cur ? window.OSTEP_PARTS[cur.part] : null;

    const themeBtn = h("button", { title: "切换主题" });
    const themes = ["system", "light", "dark"], names = { system: "◐ 跟随系统", light: "☀ 浅色", dark: "☾ 深色" };
    const paintTheme = () => (themeBtn.textContent = names[LS.get("ostep-theme", "system")]);
    themeBtn.onclick = () => { const t = themes[(themes.indexOf(LS.get("ostep-theme", "system")) + 1) % 3]; LS.set("ostep-theme", t); applyTheme(t); paintTheme(); };
    paintTheme();

    const bar = h("header", { class: "topbar" }, h("div", { class: "topbar-inner" },
      h("a", { class: "home", href: root + "index.html" }, "OSTEP 图解"),
      h("span", { class: "crumb" }, cur ? `${part.name} · 第 ${cur.n} 章 · ${cur.title}` : ""),
      idx > 0 ? h("a", { class: "btn", href: list[idx - 1].slug + ".html", title: "上一章" }, "←") : null,
      idx >= 0 && idx < list.length - 1 ? h("a", { class: "btn", href: list[idx + 1].slug + ".html", title: "下一章" }, "→") : null,
      themeBtn));
    document.body.prepend(bar);

    if (!cur) return;
    const main = document.querySelector("main");
    if (!main) return;
    // 章节头：页面若自己写了 .chapter-head 就不重复插入
    if (!main.querySelector(".chapter-head")) {
      main.prepend(h("div", { class: "chapter-head" },
        h("div", { class: "kicker" }, h("span", { class: "dot", style: { background: part.color } }), `${part.name} ${part.en}`, h("span", {}, `第 ${cur.n} 章`)),
        h("h1", {}, cur.title), h("p", { class: "en" }, cur.en)));
    }
    // 本页目录：收集 h2
    const heads = [...main.querySelectorAll("h2")];
    if (heads.length >= 3 && !main.querySelector(".toc")) {
      heads.forEach((el, k) => { if (!el.id) el.id = "s" + (k + 1); });
      const toc = h("nav", { class: "toc" }, h("b", {}, "本章目录"), h("ol", {}, heads.map((el) => h("li", {}, h("a", { href: "#" + el.id }, el.textContent.replace(/^\d+\s*/, ""))))));
      main.querySelector(".chapter-head").after(toc);
    }
    // 学完勾选 + 上下章
    const key = "ostep-done-" + cur.n;
    const cb = h("input", { type: "checkbox", id: "done-cb" });
    cb.checked = !!LS.get(key, false);
    cb.onchange = () => LS.set(key, cb.checked);
    main.append(h("label", { class: "done-box", for: "done-cb" }, cb, "我已经学完这一章（进度会显示在总览页）"));
    const prev = list[idx - 1], next = list[idx + 1];
    main.append(h("nav", { class: "chapter-nav" },
      prev ? h("a", { class: "prev", href: prev.slug + ".html" }, h("small", {}, "← 上一章 · 第 " + prev.n + " 章"), prev.title) : null,
      next ? h("a", { class: "next", href: next.slug + ".html" }, h("small", {}, "下一章 · 第 " + next.n + " 章 →"), next.title) : null));
    document.title = `${cur.n}. ${cur.title} · OSTEP 图解`;
  }

  // 选择题：<div class="mcq" data-answer="1"><div class="q">…</div><div class="opts"><button>…</button>…</div><div class="explain">…</div></div>
  function wireMcq(root) {
    root.querySelectorAll(".mcq").forEach((q) => {
      if (q.dataset.wired) return;
      q.dataset.wired = 1;
      const ans = Number(q.dataset.answer);
      const btns = [...q.querySelectorAll(".opts button")];
      btns.forEach((b, k) => b.addEventListener("click", () => {
        if (q.classList.contains("answered")) return;
        q.classList.add("answered");
        b.classList.add(k === ans ? "right" : "wrong");
        btns[ans] && btns[ans].classList.add("right");
        q.dataset.correct = k === ans ? "1" : "0";
        updateScore();
      }));
    });
  }
  function updateScore() {
    const all = document.querySelectorAll(".mcq"), done = document.querySelectorAll(".mcq.answered"), right = document.querySelectorAll('.mcq[data-correct="1"]');
    document.querySelectorAll(".quiz-score").forEach((el) => (el.textContent = `已答 ${done.length} / ${all.length}，答对 ${right.length}`));
  }
  // 实验块：从 "cd labs/<slug>" 里认出实验目录，在块首加上 GitHub 源码链接和获取方式
  const REPO = "https://github.com/shakespark/ostep-illustrated";
  function wireLabs(root) {
    root.querySelectorAll(".lab").forEach((lab) => {
      const m = lab.textContent.match(/labs\/([0-9a-z-]+)/);
      if (!m || lab.querySelector(".lab-src")) return;
      const ext = { target: "_blank", rel: "noopener" };
      lab.prepend(h("p", { class: "lab-src" },
        "代码：", h("a", { href: `${REPO}/tree/main/labs/${m[1]}`, ...ext }, `GitHub 上的 labs/${m[1]} ↗`),
        "。下面的命令在仓库根目录运行，先用 ", h("code", {}, `git clone ${REPO}.git`),
        " 或", h("a", { href: `${REPO}/archive/refs/heads/main.zip` }, "下载 zip"), " 获取整个仓库。"));
    });
  }
  function wireCopy(root) {
    root.querySelectorAll("pre").forEach((pre) => {
      if (pre.querySelector(".copy")) return;
      const b = h("button", { class: "copy", title: "复制" }, "复制");
      b.onclick = () => {
        const text = (pre.querySelector("code") || pre).innerText.replace(/^复制$/m, "");
        const ok = () => { b.textContent = "已复制"; setTimeout(() => (b.textContent = "复制"), 1200); };
        // 通过 WSL IP 等非安全上下文访问时没有 clipboard API，退回到 execCommand
        const fallback = () => {
          const ta = h("textarea", { style: { position: "fixed", opacity: "0" } }, text);
          document.body.append(ta); ta.select();
          try { document.execCommand("copy"); ok(); } catch (e) { b.textContent = "复制失败"; }
          ta.remove();
        };
        if (navigator.clipboard) navigator.clipboard.writeText(text).then(ok, fallback);
        else fallback();
      };
      pre.append(b);
    });
  }

  document.addEventListener("DOMContentLoaded", () => {
    mountChrome();
    wireMcq(document);
    wireLabs(document);
    wireCopy(document);
    updateScore();
  });

  window.OSTEP = { h, s, color, letter, rng, hex, bin, stepper, gantt, wireMcq, LS };
})();
