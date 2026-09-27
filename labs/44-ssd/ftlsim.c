// ftlsim.c —— 第 44 章实验：一个小型 FTL（闪存转换层）模拟器
//
// 模拟三种 FTL 在不同写负载下的表现：
//   direct : 直接映射（逻辑页 N → 物理页 N），覆盖写要"读出整块 → 擦除 → 重编程"
//   log    : 页映射 + 日志结构（追加写到当前块），空闲块不足时做贪心垃圾回收（GC）
//   log+wl : 在 log 基础上加一个最简单的静态磨损均衡（wear leveling）
//
// 统计：主机写页数、闪存编程(program)/擦除(erase)/读(read)次数、写放大（WA）、
//       各块擦除次数的最小/最大值（反映磨损是否均匀）、按原书 Figure 44.2 MLC 延迟估算的总时间。
//
// 用法： ./ftlsim                 跑默认实验矩阵（3 种负载 × 3 种 FTL）
//        ./ftlsim -op              额外做"预留空间（overprovisioning）比例 vs 写放大"扫描
//        ./ftlsim -b 256 -p 64 -n 200000 -s 1   修改块数、每块页数、写次数、随机种子
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { PG_INVALID = 0, PG_ERASED = 1, PG_VALID = 2 };
enum { FTL_DIRECT, FTL_LOG, FTL_LOG_WL };
enum { WL_SEQ, WL_RAND, WL_HOT, WL_COLD };

// 原书 Figure 44.2 中 MLC 闪存的大致延迟（微秒）
#define T_READ    50.0
#define T_PROGRAM 750.0
#define T_ERASE   3000.0

typedef struct {
    int nblocks, ppb, npages;   // 物理块数、每块页数、物理页总数
    int nlogical;               // 暴露给主机的逻辑页数
    int type;
    unsigned char *state;       // 每个物理页的状态
    int *owner;                 // 物理页 → 存放的逻辑页（-1 表示无）
    int *map;                   // 逻辑页 → 物理页（-1 表示未映射）
    int *valid;                 // 每块中"存活"页数
    int *erases;                // 每块擦除次数（磨损）
    int *freelist, nfree;       // 空闲块栈（log FTL）
    int frontier, fpos;         // 当前追加块 & 块内下一页
    long host_writes, programs, erase_ops, reads;
    long gc_programs, wl_programs;
} ftl_t;

static unsigned long long rng_state = 1;
static unsigned rnd(void) {   // xorshift64*，可复现
    rng_state ^= rng_state >> 12; rng_state ^= rng_state << 25; rng_state ^= rng_state >> 27;
    return (unsigned)((rng_state * 2685821657736338717ULL) >> 32);
}

static ftl_t *ftl_new(int type, int nblocks, int ppb, double op) {
    ftl_t *f = calloc(1, sizeof *f);
    f->type = type; f->nblocks = nblocks; f->ppb = ppb; f->npages = nblocks * ppb;
    // direct 映射没有额外空间可用：逻辑页数 = 物理页数；log 类按预留比例 op 缩小逻辑容量
    f->nlogical = (type == FTL_DIRECT) ? f->npages : (int)(f->npages * (1.0 - op));
    f->state = calloc(f->npages, 1);
    f->owner = malloc(sizeof(int) * f->npages);
    f->map = malloc(sizeof(int) * f->nlogical);
    f->valid = calloc(nblocks, sizeof(int));
    f->erases = calloc(nblocks, sizeof(int));
    f->freelist = malloc(sizeof(int) * nblocks);
    for (int i = 0; i < f->npages; i++) f->owner[i] = -1;
    for (int i = 0; i < f->nlogical; i++) f->map[i] = -1;
    for (int b = nblocks - 1; b >= 0; b--) f->freelist[f->nfree++] = b;  // 初始全部 INVALID，算"空闲"
    f->frontier = -1; f->fpos = ppb;
    return f;
}

static void ftl_free(ftl_t *f) {
    free(f->state); free(f->owner); free(f->map); free(f->valid); free(f->erases); free(f->freelist); free(f);
}

static void erase_block(ftl_t *f, int b) {
    for (int i = 0; i < f->ppb; i++) { f->state[b * f->ppb + i] = PG_ERASED; f->owner[b * f->ppb + i] = -1; }
    f->erases[b]++; f->erase_ops++; f->valid[b] = 0;
}

static void program_page(ftl_t *f, int ppn, int lpn) {
    if (f->state[ppn] != PG_ERASED) { fprintf(stderr, "BUG: program 未擦除页 %d\n", ppn); exit(1); }
    f->state[ppn] = PG_VALID; f->owner[ppn] = lpn; f->programs++;
}

// ---------------- direct mapped ----------------
static void direct_write(ftl_t *f, int lpn) {
    int ppn = lpn, b = ppn / f->ppb;
    if (f->state[ppn] == PG_ERASED) {            // 目标页恰好是已擦除状态：直接编程
        program_page(f, ppn, lpn);
    } else {                                     // 读-改-写：读出块内其他有效页、擦除、全部重编程
        int keep[4096], nk = 0;
        for (int i = 0; i < f->ppb; i++) {
            int p = b * f->ppb + i;
            if (p != ppn && f->state[p] == PG_VALID) { keep[nk++] = p; f->reads++; }
        }
        erase_block(f, b);
        for (int k = 0; k < nk; k++) program_page(f, keep[k], keep[k]);
        program_page(f, ppn, lpn);
    }
    f->map[lpn] = ppn;
}

// ---------------- log-structured ----------------
static int pop_free(ftl_t *f) {
    if (f->nfree == 0) { fprintf(stderr, "设备空间耗尽（GC 无法回收）\n"); exit(1); }
    int b = f->freelist[--f->nfree];
    if (f->state[b * f->ppb] != PG_ERASED || f->valid[b] != 0) erase_block(f, b);
    else {  // 已擦除过的块直接用；但初始 INVALID 的块必须先擦
        for (int i = 0; i < f->ppb; i++) if (f->state[b * f->ppb + i] != PG_ERASED) { erase_block(f, b); break; }
    }
    return b;
}

static void append(ftl_t *f, int lpn, long *counter) {
    if (f->fpos == f->ppb) { f->frontier = pop_free(f); f->fpos = 0; }
    int ppn = f->frontier * f->ppb + f->fpos++;
    int old = f->map[lpn];
    if (old >= 0) { f->valid[old / f->ppb]--; f->owner[old] = -1; }   // 旧版本变成垃圾
    program_page(f, ppn, lpn);
    f->map[lpn] = ppn; f->valid[f->frontier]++;
    if (counter) (*counter)++;
}

// 把块 b 中所有存活页迁移到日志末尾，再擦除 b 放回空闲池
static void migrate_block(ftl_t *f, int b, long *counter) {
    for (int i = 0; i < f->ppb; i++) {
        int p = b * f->ppb + i;
        if (f->state[p] == PG_VALID && f->owner[p] >= 0 && f->map[f->owner[p]] == p) {
            f->reads++;
            append(f, f->owner[p], counter);
        }
    }
    erase_block(f, b);
    f->freelist[f->nfree++] = b;
}

static int is_candidate(ftl_t *f, int b) {
    if (b == f->frontier) return 0;
    for (int k = 0; k < f->nfree; k++) if (f->freelist[k] == b) return 0;
    return 1;
}

static void maybe_wear_level(ftl_t *f) {
    // 静态磨损均衡：若"最年轻"的块比"最老"的块少擦除 THRESH 次以上，
    // 说明它装着长期不变的冷数据，把它的数据搬走，让它重新参与写入轮转。
    const int THRESH = 30;
    int mn = -1, mx = 0;
    for (int b = 0; b < f->nblocks; b++) {
        if (f->erases[b] > mx) mx = f->erases[b];
        if (is_candidate(f, b) && (mn < 0 || f->erases[b] < f->erases[mn])) mn = b;
    }
    if (mn >= 0 && mx - f->erases[mn] > THRESH) migrate_block(f, mn, &f->wl_programs);
}

static void gc(ftl_t *f) {
    // 空闲块少于 2 个时启动：贪心选择存活页最少的块作为受害者
    int guard = 0;
    while (f->nfree < 2 && guard++ < f->nblocks * 4) {
        int victim = -1;
        for (int b = 0; b < f->nblocks; b++)
            if (is_candidate(f, b) && (victim < 0 || f->valid[b] < f->valid[victim])) victim = b;
        if (victim < 0 || f->valid[victim] == f->ppb) break;
        migrate_block(f, victim, &f->gc_programs);
        if (f->type == FTL_LOG_WL) maybe_wear_level(f);
    }
}

static void log_write(ftl_t *f, int lpn) {
    if (f->fpos == f->ppb) gc(f);   // 需要新块前，确保空闲池充足
    append(f, lpn, NULL);
}

static void host_write(ftl_t *f, int lpn) {
    f->host_writes++;
    if (f->type == FTL_DIRECT) direct_write(f, lpn); else log_write(f, lpn);
}

static int pick(ftl_t *f, int wl, long i) {
    switch (wl) {
    case WL_SEQ:  return (int)(i % f->nlogical);
    case WL_RAND: return (int)(rnd() % (unsigned)f->nlogical);
    case WL_COLD: // 只改写前 10% 的逻辑页，其余 90% 是写一次就不再动的冷数据
        return (int)(rnd() % (unsigned)(f->nlogical / 10));
    default: {    // 80/20：80% 的写落在前 20% 的逻辑页
        int hot = f->nlogical / 5;
        if (rnd() % 100 < 80) return (int)(rnd() % (unsigned)hot);
        return hot + (int)(rnd() % (unsigned)(f->nlogical - hot));
    }
    }
}

typedef struct { long hw, prog, er, rd; double wa; int emin, emax; double ms; } result_t;

static result_t run(int type, int wl, int nblocks, int ppb, double op, long nops, unsigned long long seed) {
    rng_state = seed * 0x9E3779B97F4A7C15ULL + 1;
    ftl_t *f = ftl_new(type, nblocks, ppb, op);
    for (int l = 0; l < f->nlogical; l++) host_write(f, l);           // 预热：先把盘写满一遍
    long hw0 = f->host_writes, p0 = f->programs, e0 = f->erase_ops, r0 = f->reads;
    int *e_before = malloc(sizeof(int) * nblocks);
    memcpy(e_before, f->erases, sizeof(int) * nblocks);
    for (long i = 0; i < nops; i++) host_write(f, pick(f, wl, i));   // 测量阶段
    result_t r;
    r.hw = f->host_writes - hw0; r.prog = f->programs - p0; r.er = f->erase_ops - e0; r.rd = f->reads - r0;
    r.wa = (double)r.prog / (double)r.hw;
    r.emin = 1 << 30; r.emax = 0;
    for (int b = 0; b < nblocks; b++) {
        int d = f->erases[b] - e_before[b];
        if (d < r.emin) r.emin = d;
        if (d > r.emax) r.emax = d;
    }
    r.ms = (r.rd * T_READ + r.prog * T_PROGRAM + r.er * T_ERASE) / 1000.0;
    free(e_before); ftl_free(f);
    return r;
}

int main(int argc, char **argv) {
    int nblocks = 256, ppb = 64; long nops = 200000; unsigned long long seed = 1; int opsweep = 0;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-b") && i + 1 < argc) nblocks = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-p") && i + 1 < argc) ppb = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-n") && i + 1 < argc) nops = atol(argv[++i]);
        else if (!strcmp(argv[i], "-s") && i + 1 < argc) seed = strtoull(argv[++i], 0, 10);
        else if (!strcmp(argv[i], "-op")) opsweep = 1;
        else { fprintf(stderr, "用法: %s [-b 块数] [-p 每块页数] [-n 写次数] [-s 种子] [-op]\n", argv[0]); return 1; }
    }
    if (ppb > 4096 || nblocks < 8) { fprintf(stderr, "参数超出范围\n"); return 1; }
    const char *wlname[] = { "seq", "random", "hot80/20", "cold90" };
    const char *ftlname[] = { "direct", "log", "log+wl" };
    double op = 0.10;
    printf("闪存：%d 块 × %d 页/块 = %d 页；log 类 FTL 预留 %.0f%% 空间；预热写满后再测 %ld 次写\n",
           nblocks, ppb, nblocks * ppb, op * 100, nops);
    printf("延迟按原书 Figure 44.2 的 MLC：读 %.0fus、编程 %.0fus、擦除 %.0fus\n\n", T_READ, T_PROGRAM, T_ERASE);
    printf("负载：seq=顺序覆盖  random=均匀随机  hot80/20=80%%写落在20%%逻辑页  cold90=只改写10%%逻辑页(其余是冷数据)\n");
    printf("列：host=主机写页数 prog=闪存编程 erase=擦除 read=GC/改写引起的闪存读 WA=写放大 wear=各块擦除次数min/max time=估算秒\n\n");
    printf("%-9s %-7s %8s %9s %7s %9s %6s %9s %8s\n",
           "workload", "FTL", "host", "prog", "erase", "read", "WA", "wear", "time(s)");
    for (int wl = 0; wl < 4; wl++) {
        for (int t = 0; t < 3; t++) {
            result_t r = run(t, wl, nblocks, ppb, op, nops, seed);
            char mm[32]; snprintf(mm, sizeof mm, "%d/%d", r.emin, r.emax);
            printf("%-9s %-7s %8ld %9ld %7ld %9ld %6.2f %9s %8.1f\n",
                   wlname[wl], ftlname[t], r.hw, r.prog, r.er, r.rd, r.wa, mm, r.ms / 1000.0);
        }
    }
    if (opsweep) {
        printf("\n预留空间比例 vs 写放大（log FTL，均匀随机写）\n");
        printf("%8s %8s %8s\n", "OP", "WA", "erase");
        double ops[] = { 0.05, 0.07, 0.10, 0.15, 0.20, 0.28, 0.40, 0.50 };
        for (unsigned k = 0; k < sizeof ops / sizeof ops[0]; k++) {
            result_t r = run(FTL_LOG, WL_RAND, nblocks, ppb, ops[k], nops, seed);
            printf("%7.0f%% %8.2f %8ld\n", ops[k] * 100, r.wa, r.er);
        }
    }
    return 0;
}
