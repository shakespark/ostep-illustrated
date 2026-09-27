// afs_sim.c —— AFS 缓存一致性的小型模拟器（与网页上的时间线模拟器逻辑相同）
//   第 1 部分：逐行执行原书 Figure 50.3，打印两台客户端的缓存、服务器内容和每次 read 读到的值
//   第 2 部分：枚举两个客户端各 3 步的全部 20 种交错（类似 afs.py 作业第 4~6 题），
//             分别在 AFS 语义和 NFS 语义（属性缓存超时 T=0）下统计结果
// 用法：./afs_sim [1|2]
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SUSPECT_ON_RECOVER 1       // 客户端/服务器恢复后是否把缓存视为可疑（TestAuth）

enum { AFS, NFS };
enum { OPEN, READ, WRITE, CLOSE, DOWN, UP, SCRASH };
typedef struct { int proc; int op; char val; } op_t;  // proc: 0..3 = P1..P4；val: write 的值

typedef struct { int has; char val; int ver, valid, suspect, mtime; } cache_t;
typedef struct { cache_t c; int dirty, up; int attr_has, attr_mtime, attr_at; } client_t;
typedef struct { int open, wrote, open_stale; } proc_t;

static int mode, T;
static struct { char val; int exists, ver, mtime; int cb[2]; } srv;
static client_t cl[2];
static proc_t pr[4];
static int now;
static int msg_fetch, msg_store, msg_break, msg_testauth;

static int client_of(int p) { return p < 2 ? 0 : 1; }
static char show(char v) { return v ? v : '-'; }

static void reset(char init) {
    memset(&srv, 0, sizeof(srv)); memset(cl, 0, sizeof(cl)); memset(pr, 0, sizeof(pr));
    cl[0].up = cl[1].up = 1;
    if (init) { srv.val = init; srv.exists = 1; srv.ver = 1; }
    now = 0; msg_fetch = msg_store = msg_break = msg_testauth = 0;
}
static void fetch(int c) {
    msg_fetch++;
    cl[c].c.has = 1; cl[c].c.val = srv.val; cl[c].c.ver = srv.ver; cl[c].c.valid = 1; cl[c].c.suspect = 0;
    srv.cb[c] = 1;                                   // 服务器登记回调
}
static void nfs_validate(int c) {
    client_t *x = &cl[c];
    if (!(x->attr_has && now - x->attr_at < T)) { x->attr_has = 1; x->attr_mtime = srv.mtime; x->attr_at = now; }
    if (x->c.has && !x->dirty && x->attr_mtime > x->c.mtime) x->c.has = 0;   // 作废缓存
}
// 执行一个操作；若是 read，返回读到的值（否则返回 0）
static char step(op_t o) {
    char ret = 0;
    if (o.op == DOWN || o.op == UP) {
        int c = o.proc;
        cl[c].up = (o.op == UP);
        if (o.op == UP && mode == AFS && SUSPECT_ON_RECOVER && cl[c].c.has) cl[c].c.suspect = 1;
        now++; return 0;
    }
    if (o.op == SCRASH) {
        if (mode == AFS) { srv.cb[0] = srv.cb[1] = 0; if (SUSPECT_ON_RECOVER) for (int c = 0; c < 2; c++) if (cl[c].c.has) cl[c].c.suspect = 1; }
        now++; return 0;
    }
    int c = client_of(o.proc);
    client_t *x = &cl[c];
    proc_t *p = &pr[o.proc];
    if (!x->up) { now++; return 0; }
    switch (o.op) {
    case OPEN:
        if (mode == AFS) {
            if (x->dirty) ;                                        // 本机进程正在改：用本地副本
            else if (x->c.has && x->c.valid && !x->c.suspect) ;    // 回调有效：纯本地
            else if (x->c.has && x->c.valid && x->c.suspect) {     // 可疑：TestAuth
                msg_testauth++;
                if (x->c.ver == srv.ver) { x->c.suspect = 0; srv.cb[c] = 1; } else fetch(c);
            } else fetch(c);
            p->open_stale = x->c.has && !x->dirty && x->c.ver < srv.ver;
        } else if (!x->dirty) {
            nfs_validate(c);
            if (!srv.exists) { srv.exists = 1; srv.mtime = now; x->c.has = 1; x->c.val = 0; x->c.ver = srv.ver; x->c.mtime = now; x->attr_has = 1; x->attr_mtime = now; x->attr_at = now; }
        }
        p->open = 1;
        break;
    case READ:
        if (mode == NFS && !x->dirty) {
            nfs_validate(c);
            if (!x->c.has) { x->c.has = 1; x->c.val = srv.val; x->c.ver = srv.ver; x->c.mtime = srv.mtime; x->attr_has = 1; x->attr_mtime = srv.mtime; x->attr_at = now; }
        }
        ret = x->c.has ? show(x->c.val) : '-';
        break;
    case WRITE:
        if (!x->c.has) { x->c.has = 1; x->c.ver = srv.ver; x->c.valid = 1; x->c.suspect = 0; x->c.mtime = srv.mtime; }
        x->c.val = o.val; x->dirty = 1; p->wrote = 1;
        break;
    case CLOSE:
        if (p->wrote) {
            srv.val = x->c.val; srv.exists = 1; srv.ver++; x->c.ver = srv.ver;
            if (mode == AFS) {
                msg_store++;
                x->c.valid = 1; x->c.suspect = 0;
                for (int o2 = 0; o2 < 2; o2++) {
                    if (o2 == c || !srv.cb[o2]) continue;
                    if (cl[o2].up) { if (cl[o2].c.has) cl[o2].c.valid = 0; msg_break++; }  // 打断回调
                }
                srv.cb[0] = srv.cb[1] = 0; srv.cb[c] = 1;
            } else {
                srv.mtime = now; x->c.mtime = now; x->attr_has = 1; x->attr_mtime = now; x->attr_at = now;
            }
            p->wrote = 0;
            x->dirty = 0;
            for (int q = 0; q < 4; q++) if (client_of(q) == c && pr[q].wrote) x->dirty = 1;
        }
        p->open = 0;
        break;
    }
    now++;
    return ret;
}
static void cache_str(int c, char *buf) {
    cache_t *k = &cl[c].c;
    if (!k->has) { strcpy(buf, "-"); return; }
    int struck = mode == AFS && !k->valid && !cl[c].dirty;
    sprintf(buf, "%s%c", struck ? "~" : "", show(k->val));
}

// ------------------------------ 第 1 部分 ------------------------------
static const char *opname[] = { "open(F)", "read()", "write", "close()" };
static void part1(void) {
    // 原书 Figure 50.3：P1、P2 在 Client1，P3 在 Client2
    op_t fig[] = {
        {0,OPEN,0},{0,WRITE,'A'},{0,CLOSE,0},{1,OPEN,0},{1,READ,0},{1,CLOSE,0},
        {0,OPEN,0},{0,WRITE,'B'},{1,OPEN,0},{1,READ,0},{1,CLOSE,0},
        {2,OPEN,0},{2,READ,0},{2,CLOSE,0},{0,CLOSE,0},{2,OPEN,0},{2,READ,0},{2,CLOSE,0},
        {2,OPEN,0},{0,OPEN,0},{0,WRITE,'D'},{2,WRITE,'C'},{2,CLOSE,0},{0,CLOSE,0},
        {2,OPEN,0},{2,READ,0},{2,CLOSE,0},
    };
    int n = sizeof(fig) / sizeof(fig[0]);
    mode = AFS; reset(0);
    printf("=== Figure 50.3: AFS cache consistency timeline (~X = callback broken) ===\n");
    printf("%-3s %-13s %-13s %-6s %-13s %-6s %-6s\n", "#", "P1", "P2", "C1", "P3", "C2", "Server");
    for (int i = 0; i < n; i++) {
        char r = step(fig[i]);
        char cell[3][24] = { "", "", "" }, c1[4], c2[4];
        int col = fig[i].proc;
        if (fig[i].op == WRITE) sprintf(cell[col], "write(%c)", fig[i].val);
        else if (fig[i].op == READ) sprintf(cell[col], "read() -> %c", r);
        else strcpy(cell[col], opname[fig[i].op]);
        cache_str(0, c1); cache_str(1, c2);
        printf("%-3d %-13s %-13s %-6s %-13s %-6s %-6c\n", i + 1, cell[0], cell[1], c1, cell[2], c2, show(srv.val));
    }
    printf("messages: Fetch=%d Store=%d callback-breaks=%d TestAuth=%d\n\n", msg_fetch, msg_store, msg_break, msg_testauth);
}

// ------------------------------ 第 2 部分 ------------------------------
static int schedules[32][6], nsched;
static void gen(int *s, int len, int a, int b) {
    if (a == 3 && b == 3) { memcpy(schedules[nsched++], s, sizeof(int) * 6); return; }
    if (a < 3) { s[len] = 0; gen(s, len + 1, a + 1, b); }
    if (b < 3) { s[len] = 1; gen(s, len + 1, a, b + 1); }
}
static char run_sched(int *s, int w, int m) {
    op_t A[3] = { {0,OPEN,0}, {0,WRITE, w == 1 ? '1' : 'X'}, {0,CLOSE,0} };
    op_t B[3] = { {2,OPEN,0}, w == 1 ? (op_t){2,READ,0} : (op_t){2,WRITE,'Y'}, {2,CLOSE,0} };
    mode = m; T = 0; reset('0');
    int a = 0, b = 0; char got = 0;
    for (int i = 0; i < 6; i++) {
        op_t o = s[i] == 0 ? A[a++] : B[b++];
        char r = step(o);
        if (o.op == READ) got = r;
    }
    return w == 1 ? got : srv.val;
}
static void part2(void) {
    int s[6];
    nsched = 0; gen(s, 0, 0, 0);
    for (int w = 1; w <= 2; w++) {
        printf("=== Workload %d: client0 = open,write %s,close   client1 = open,%s,close  (file starts as 0) ===\n",
               w, w == 1 ? "1" : "X", w == 1 ? "read" : "write Y");
        printf("schedule  AFS  NFS(T=0)\n");
        int cntA[128] = { 0 }, cntN[128] = { 0 };
        for (int k = 0; k < nsched; k++) {
            char a = run_sched(schedules[k], w, AFS), n = run_sched(schedules[k], w, NFS);
            cntA[(int) a]++; cntN[(int) n]++;
            for (int i = 0; i < 6; i++) putchar('0' + schedules[k][i]);
            printf("    %c    %c\n", a, n);
        }
        printf("summary (%d interleavings):", nsched);
        for (int v = 0; v < 128; v++) if (cntA[v]) printf("  AFS %c x%d", v, cntA[v]);
        for (int v = 0; v < 128; v++) if (cntN[v]) printf("  NFS %c x%d", v, cntN[v]);
        printf("\n\n");
    }
}

int main(int argc, char *argv[]) {
    int which = argc > 1 ? atoi(argv[1]) : 0;
    if (!which || which == 1) part1();
    if (!which || which == 2) part2();
    return 0;
}
