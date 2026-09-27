#!/usr/bin/env python3
"""lfs_mini.py —— 一个几百行以内的日志结构文件系统（LFS）小模拟器。

思路仿 ostep-homework 的 lfs.py：
  * 地址 0 是检查点区域 CR，记录每个 imap 片段的最新地址；
  * 其余地址按段（segment）划分，所有更新（数据块、inode、目录块、imap 片段）只追加到日志末尾；
  * 每段第 0 块是段摘要块 SS，记录本段每个块属于哪个 inode 的第几块；
  * 清理器按书中伪代码 (N, T) = SS[A]; inode = imap[N]; inode[T] == A ? 活 : 死 判断存活并压缩段。

用法：
  python3 lfs_mini.py -L "c,/foo:w,/foo,0,1" -c        # 执行命令并显示存活性
  python3 lfs_mini.py --demo-clean                     # 反复覆盖写 → 清理最空的两个段
命令格式：c,/路径  d,/目录  w,/路径,起始块,块数  r,/路径  l,/已有,/新名
"""
import argparse
import copy

SEG, NSEG, IPC, NINODE, NPTR = 8, 8, 16, 32, 8


class LFS:
    def __init__(self):
        self.disk = [None] * (1 + SEG * NSEG)
        self.imap = [-1] * NINODE          # 内存中的 imap：inode 号 -> 磁盘地址
        self.seg_used = [False] * NSEG
        self.cur, self.head, self.ver = 0, 1, 0
        self.cr_chunks = [-1] * (NINODE // IPC)
        d = self.append({"kind": "dir", "inum": 0, "off": 0, "entries": [[".", 0], ["..", 0]]})
        self.imap[0] = self.append({"kind": "inode", "inum": 0, "type": "dir", "size": 1, "refs": 2,
                                    "ptrs": [d] + [-1] * (NPTR - 1)})
        self.write_chunks({0})

    # ---------- 日志追加 ----------
    def seg_start(self, s):
        return 1 + s * SEG

    def append(self, b):
        if self.head >= self.seg_start(self.cur) + SEG:
            # 从当前段往后找下一个空闲段（与网页模拟器一致）
            free = [(self.cur + k) % NSEG for k in range(1, NSEG + 1) if not self.seg_used[(self.cur + k) % NSEG]]
            if not free:
                raise RuntimeError("磁盘已满：需要清理")
            self.cur = free[0]
            self.head = self.seg_start(self.cur)
        self.seg_used[self.cur] = True
        if self.head == self.seg_start(self.cur):          # 段首放段摘要 SS
            self.disk[self.head] = {"kind": "ss", "entries": {}}
            self.head += 1
        a = self.head
        self.head += 1
        self.disk[a] = b
        ss = self.disk[self.seg_start(self.cur)]["entries"]
        ss[a] = ("imap", b["chunk"]) if b["kind"] == "imap" else (b["kind"], b["inum"], b.get("off"))
        return a

    def write_chunks(self, chunks):
        for c in sorted(chunks):
            a = self.append({"kind": "imap", "chunk": c, "map": self.imap[c * IPC:(c + 1) * IPC]})
            self.cr_chunks[c] = a
        self.disk[0] = {"kind": "cr", "imap": list(self.cr_chunks)}   # 这里每条命令都更新 CR

    def inode(self, n):
        return self.disk[self.imap[n]]

    # ---------- 目录与路径 ----------
    def lookup(self, path):
        n = 0
        for p in [x for x in path.split("/") if x]:
            ino = self.inode(n)
            if ino["type"] != "dir":
                return None
            hit = [e for e in self.disk[ino["ptrs"][0]]["entries"] if e[0] == p]
            if not hit:
                return None
            n = hit[0][1]
        return n

    def split(self, path):
        parts = [x for x in path.split("/") if x]
        return "/" + "/".join(parts[:-1]), parts[-1]

    def rewrite_parent(self, pn, fn, dref=0):
        pino = copy.deepcopy(self.inode(pn))
        entries = copy.deepcopy(self.disk[pino["ptrs"][0]]["entries"])
        fn(entries)
        pino["ptrs"][0] = self.append({"kind": "dir", "inum": pn, "off": 0, "entries": entries})
        pino["refs"] += dref
        self.imap[pn] = self.append(pino)

    def alloc(self):
        return next(n for n in range(NINODE) if self.imap[n] < 0)

    # ---------- 命令 ----------
    def run(self, cmd):
        f = [x.strip() for x in cmd.split(",")]
        op = f[0]
        if op in ("c", "d"):
            parent, name = self.split(f[1])
            pn, n = self.lookup(parent), self.alloc()
            if op == "c":
                self.imap[n] = self.append({"kind": "inode", "inum": n, "type": "reg", "size": 0, "refs": 1,
                                            "ptrs": [-1] * NPTR})
                self.rewrite_parent(pn, lambda es: es.append([name, n]))
            else:
                d = self.append({"kind": "dir", "inum": n, "off": 0, "entries": [[".", n], ["..", pn]]})
                self.imap[n] = self.append({"kind": "inode", "inum": n, "type": "dir", "size": 1, "refs": 2,
                                            "ptrs": [d] + [-1] * (NPTR - 1)})
                self.rewrite_parent(pn, lambda es: es.append([name, n]), 1)
            self.write_chunks({n // IPC, pn // IPC})
        elif op == "w":
            n, off, cnt = self.lookup(f[1]), int(f[2]), int(f[3])
            ino = copy.deepcopy(self.inode(n))
            self.ver += 1
            for k in range(off, off + cnt):
                ino["ptrs"][k] = self.append({"kind": "data", "inum": n, "off": k, "ver": self.ver})
            ino["size"] = max(ino["size"], off + cnt)
            self.imap[n] = self.append(ino)
            self.write_chunks({n // IPC})
        elif op == "r":
            parent, name = self.split(f[1])
            pn, n = self.lookup(parent), self.lookup(f[1])
            ino = copy.deepcopy(self.inode(n))
            self.rewrite_parent(pn, lambda es: es.remove([e for e in es if e[0] == name][0]),
                                -1 if ino["type"] == "dir" else 0)
            if ino["type"] == "dir" or ino["refs"] <= 1:
                self.imap[n] = -1
            else:
                ino["refs"] -= 1
                self.imap[n] = self.append(ino)
            self.write_chunks({n // IPC, pn // IPC})
        elif op == "l":
            n = self.lookup(f[1])
            parent, name = self.split(f[2])
            pn = self.lookup(parent)
            self.rewrite_parent(pn, lambda es: es.append([name, n]))
            ino = copy.deepcopy(self.inode(n))
            ino["refs"] += 1
            self.imap[n] = self.append(ino)
            self.write_chunks({n // IPC, pn // IPC})
        else:
            raise ValueError("不认识的命令 " + cmd)

    # ---------- 存活性与清理 ----------
    def live_set(self):
        live = {0}
        for ca in self.disk[0]["imap"]:
            if ca < 0:
                continue
            live.add(ca)
            for ia in self.disk[ca]["map"]:
                if ia >= 0:
                    live.add(ia)
                    live.update(p for p in self.disk[ia]["ptrs"] if p >= 0)
        return live

    def check(self, a):
        """书中伪代码：(N,T) = SegmentSummary[A]; inode = Read(imap[N]); inode[T] == A ?"""
        e = self.disk[self.seg_start((a - 1) // SEG)]["entries"][a]
        if e[0] == "imap":
            return self.cr_chunks[e[1]] == a, e
        kind, n, t = e
        if kind == "inode":
            return self.imap[n] == a, e
        return self.imap[n] >= 0 and self.inode(n)["ptrs"][t] == a, e

    def clean(self, segs):
        live = []
        for s in segs:
            for a in range(self.seg_start(s) + 1, self.seg_start(s) + SEG):
                if self.disk[a] is not None:
                    ok, e = self.check(a)
                    print(f"  检查 [{a:3}] SS={e} -> {'活' if ok else '死'}")
                    if ok:
                        live.append(a)
        if self.cur in segs:
            self.head = self.seg_start(self.cur) + SEG
        pend, chunks = {}, set()
        for a in live:
            b = self.disk[a]
            if b["kind"] in ("data", "dir"):
                na = self.append(copy.deepcopy(b))
                pend.setdefault(b["inum"], copy.deepcopy(self.inode(b["inum"])))["ptrs"][b["off"]] = na
            elif b["kind"] == "inode":
                pend.setdefault(b["inum"], copy.deepcopy(self.inode(b["inum"])))
            elif b["kind"] == "imap":
                chunks.add(b["chunk"])
        for n in sorted(pend):
            self.imap[n] = self.append(pend[n])
            chunks.add(n // IPC)
        self.write_chunks(chunks)
        for s in segs:
            for a in range(self.seg_start(s), self.seg_start(s) + SEG):
                self.disk[a] = None
            self.seg_used[s] = False
        return len(live)

    def seg_util(self):
        live = self.live_set()
        out = []
        for s in range(NSEG):
            blocks = [a for a in range(self.seg_start(s) + 1, self.seg_start(s) + SEG) if self.disk[a]]
            out.append((s, sum(a in live for a in blocks), len(blocks)))
        return out

    # ---------- 输出 ----------
    def dump(self, show_live):
        live = self.live_set()
        fmt = lambda xs: " ".join("--" if x < 0 else str(x) for x in xs)
        for a, b in enumerate(self.disk):
            if b is None:
                continue
            k = b["kind"]
            if k == "cr":
                d = "checkpoint: " + fmt(b["imap"])
            elif k == "ss":
                d = "segment summary: " + " ".join(f"{x}:({','.join('-' if y is None else str(y) for y in v)})" for x, v in b["entries"].items())
            elif k == "imap":
                d = f"chunk(imap {b['chunk']}): " + fmt(b["map"])
            elif k == "inode":
                d = f"inode {b['inum']} type:{b['type']} size:{b['size']} refs:{b['refs']} ptrs: " + fmt(b["ptrs"])
            elif k == "dir":
                d = "dir " + " ".join(f"[{e[0]},{e[1]}]" for e in b["entries"])
            else:
                d = f"data (inode {b['inum']}, off {b['off']}) v{b['ver']}"
            tag = ("live" if a in live else "    ") if show_live and k != "ss" else ("    " if show_live else "")
            print(f"[{a:3}] {tag} {d}")

    def files(self):
        out = []

        def walk(n, p):
            for name, c in self.disk[self.inode(n)["ptrs"][0]]["entries"]:
                if name in (".", ".."):
                    continue
                ino = self.inode(c)
                if ino["type"] == "dir":
                    out.append(p + name + "/")
                    walk(c, p + name + "/")
                else:
                    vers = ",".join(f"v{self.disk[x]['ver']}" for x in ino["ptrs"] if x >= 0)
                    out.append(f"{p}{name}[{vers}]")
        walk(0, "/")
        return out


def main():
    ap = argparse.ArgumentParser(description="迷你 LFS 模拟器")
    ap.add_argument("-L", default="c,/foo:w,/foo,0,1", help="冒号分隔的命令列表")
    ap.add_argument("-c", action="store_true", help="显示每个块的存活性")
    ap.add_argument("--demo-clean", action="store_true", help="演示段清理")
    args = ap.parse_args()
    fs = LFS()
    if args.demo_clean:
        cmds = "c,/a:w,/a,0,2:c,/b:w,/b,0,1:w,/a,0,1:w,/b,0,1:c,/c:w,/c,0,3:r,/b:w,/a,1,1:w,/c,2,1"
        for c in cmds.split(":"):
            fs.run(c)
        print("命令：", cmds)
        print("文件内容（每块的版本号）：", fs.files())
        print("清理前各段 活块/已用块：", " ".join(f"S{s}:{l}/{u}" for s, l, u in fs.seg_util()))
        util = fs.seg_util()
        dead = [s for s, l, u in util if u and l == 0 and s != fs.cur]
        print(f"\n第一轮：段 {dead} 里一个活块都没有，直接整段回收（不用搬任何块）")
        n = fs.clean(dead)
        print(f"搬走 {n} 个活块后：", " ".join(f"S{s}:{l}/{u}" for s, l, u in fs.seg_util()))
        util = fs.seg_util()
        victims = sorted((s for s, l, u in util if l > 0 and s != fs.cur), key=lambda s: (util[s][1], s))[:2]
        print(f"\n第二轮：选利用率最低、但还有活块的两个段 {victims} 清理")
        n = fs.clean(victims)
        print(f"搬走 {n} 个活块后：", " ".join(f"S{s}:{l}/{u}" for s, l, u in fs.seg_util()))
        print("文件内容（清理后应不变）：", fs.files())
        return
    cmds = args.L.split(":")
    for i, c in enumerate(cmds):
        fs.run(c)
        print(f"命令 {i + 1}: {c}")
    print()
    fs.dump(args.c)
    print("\n文件：", fs.files())


if __name__ == "__main__":
    main()
