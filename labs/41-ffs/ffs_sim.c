// ffs_sim.c —— 一个极简 FFS 放置模拟器（与网页里的模拟器是同一套算法）。
// 比较三种 inode 放置策略：
//   ffs    ：目录放进"空闲 inode 最多"的组；普通文件放进父目录所在的组
//   spread ：所有 inode（文件也一样）都放进空闲 inode 最多的组 —— 原书的对照策略
//   random ：随机挑一个还有空闲 inode 的组
// 数据块总是先放在 inode 所在组；大文件例外（-L）每 L 块换到下一个组。
// 输出：原书两个示例的布局图 + 一个多目录工作负载上的 dirspan / filespan 统计 + chunk 分摊表。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAXG 64
#define MAXI 64
#define MAXD 256
#define MAXF 512

typedef struct { int g, j; } Pos;
typedef struct {
    char path[64], parent[64], sym;
    int isdir, igroup, islot, nblocks, alive;
    Pos blocks[MAXD * 4];
} File;

typedef struct {
    int G, I, D, L, C;
    const char *policy;
    int inodes[MAXG][MAXI];  // 文件下标，-1 表示空闲
    int data[MAXG][MAXD];
    File files[MAXF];
    int nfiles;
    uint32_t rs;             // mulberry32 状态
} FS;

static double rnd(FS *fs) {     // mulberry32，与网页 OSTEP.rng 相同
    uint32_t t;
    fs->rs += 0x6d2b79f5u;
    t = fs->rs;
    t = (t ^ (t >> 15)) * (t | 1u);
    t = (t + ((t ^ (t >> 7)) * (t | 61u))) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}
static uint32_t wl_state;
static double wl_rnd(void) {
    uint32_t t;
    wl_state += 0x6d2b79f5u;
    t = wl_state;
    t = (t ^ (t >> 15)) * (t | 1u);
    t = (t + ((t ^ (t >> 7)) * (t | 61u))) ^ t;
    return (double)(t ^ (t >> 14)) / 4294967296.0;
}

static void fs_init(FS *fs, int G, int I, int D, int L, int C, const char *policy, uint32_t seed) {
    memset(fs, 0, sizeof *fs);
    fs->G = G; fs->I = I; fs->D = D; fs->L = L; fs->C = C; fs->policy = policy; fs->rs = seed ? seed : 1;
    for (int g = 0; g < G; g++) {
        for (int i = 0; i < I; i++) fs->inodes[g][i] = -1;
        for (int j = 0; j < D; j++) fs->data[g][j] = -1;
    }
}
static int free_inodes(FS *fs, int g) { int n = 0; for (int i = 0; i < fs->I; i++) n += fs->inodes[g][i] < 0; return n; }
static int dir_count(FS *fs, int g) {
    int n = 0;
    for (int i = 0; i < fs->I; i++) { int f = fs->inodes[g][i]; if (f >= 0 && fs->files[f].isdir) n++; }
    return n;
}
static int find(FS *fs, const char *p) {
    for (int k = 0; k < fs->nfiles; k++) if (fs->files[k].alive && !strcmp(fs->files[k].path, p)) return k;
    return -1;
}
static void parent_of(const char *p, char *out) {
    strcpy(out, p);
    char *s = strrchr(out, '/');
    if (s == out) out[1] = 0; else *s = 0;
}
static int group_most_free(FS *fs) {
    int best = -1;
    for (int g = 0; g < fs->G; g++) {
        int fg = free_inodes(fs, g);
        if (!fg) continue;
        if (best < 0 || fg > free_inodes(fs, best) || (fg == free_inodes(fs, best) && dir_count(fs, g) < dir_count(fs, best))) best = g;
    }
    return best;
}
static int inode_group(FS *fs, int isdir, const char *path) {
    if (!strcmp(path, "/")) return 0;
    if (!strcmp(fs->policy, "random")) {
        int ok[MAXG], n = 0;
        for (int g = 0; g < fs->G; g++) if (free_inodes(fs, g)) ok[n++] = g;
        return n ? ok[(int)(rnd(fs) * n)] : -1;
    }
    if (isdir || !strcmp(fs->policy, "spread")) return group_most_free(fs);
    char par[64]; parent_of(path, par);
    int pg = fs->files[find(fs, par)].igroup;
    for (int k = 0; k < fs->G; k++) { int g = (pg + k) % fs->G; if (free_inodes(fs, g)) return g; }
    return -1;
}
static int block_in_group(FS *fs, int g, Pos *prev) {
    int C = fs->C < 1 ? 1 : fs->C;
    if (prev && prev->g == g && prev->j + 1 < fs->D && fs->data[g][prev->j + 1] < 0) return prev->j + 1;
    for (int j = 0; j + C <= fs->D; j++) {
        int ok = 1;
        for (int t = 0; t < C; t++) if (fs->data[g][j + t] >= 0) { ok = 0; break; }
        if (ok) return j;
    }
    return -1;
}
static int alloc_block(FS *fs, int startg, Pos *prev, Pos *out) {
    for (int k = 0; k < fs->G; k++) {
        int g = (startg + k) % fs->G, j = block_in_group(fs, g, prev);
        if (j >= 0) { out->g = g; out->j = j; return 1; }
    }
    for (int k = 0; k < fs->G; k++) {
        int g = (startg + k) % fs->G;
        for (int j = 0; j < fs->D; j++) if (fs->data[g][j] < 0) { out->g = g; out->j = j; return 1; }
    }
    return 0;
}
static char pick_sym(FS *fs, const char *path) {
    if (!strcmp(path, "/")) return '/';
    const char *base = strrchr(path, '/') + 1;
    static const char *extra = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    char cand[128]; snprintf(cand, sizeof cand, "%s%s", base, extra);
    for (char *c = cand; *c; c++) {
        int used = 0;
        for (int k = 0; k < fs->nfiles; k++) if (fs->files[k].alive && fs->files[k].sym == *c) used = 1;
        if (!used) return *c;
    }
    return '?';
}
static int create(FS *fs, const char *path, int isdir, int size) {
    int g = inode_group(fs, isdir, path);
    if (g < 0) return -1;
    int slot = 0; while (fs->inodes[g][slot] >= 0) slot++;
    File *f = &fs->files[fs->nfiles];
    memset(f, 0, sizeof *f);
    strcpy(f->path, path);
    if (strcmp(path, "/")) parent_of(path, f->parent);
    f->sym = pick_sym(fs, path);
    f->isdir = isdir; f->igroup = g; f->islot = slot; f->alive = 1;
    fs->inodes[g][slot] = fs->nfiles++;
    int n = isdir ? 1 : size, chunkg = g;
    Pos prev, *pp = NULL;
    for (int b = 0; b < n; b++) {
        if (fs->L > 0 && b > 0 && b % fs->L == 0) { chunkg = (chunkg + 1) % fs->G; pp = NULL; }
        Pos p;
        if (!alloc_block(fs, pp ? pp->g : chunkg, pp, &p)) return -1;
        fs->data[p.g][p.j] = fs->nfiles - 1;
        f->blocks[f->nblocks++] = p;
        prev = p; pp = &prev;
    }
    return 0;
}
static void print_map(FS *fs) {
    printf("group inodes    data\n");
    for (int g = 0; g < fs->G; g++) {
        printf("%5d ", g);
        for (int i = 0; i < fs->I; i++) putchar(fs->inodes[g][i] < 0 ? '-' : fs->files[fs->inodes[g][i]].sym);
        for (int j = 0; j < fs->D; j++) {
            if (j % 10 == 0) putchar(' ');
            putchar(fs->data[g][j] < 0 ? '-' : fs->files[fs->data[g][j]].sym);
        }
        putchar('\n');
    }
}
static int addr_i(FS *fs, File *f) { return f->igroup * (fs->I + fs->D) + f->islot; }
static int addr_d(FS *fs, Pos p) { return p.g * (fs->I + fs->D) + fs->I + p.j; }
static void span_add(FS *fs, File *f, int *lo, int *hi) {
    int a = addr_i(fs, f);
    if (a < *lo) *lo = a;
    if (a > *hi) *hi = a;
    for (int b = 0; b < f->nblocks; b++) { a = addr_d(fs, f->blocks[b]); if (a < *lo) *lo = a; if (a > *hi) *hi = a; }
}
typedef struct { double avgdir, avgfile, same, avgpd; } Metrics;
static Metrics metrics(FS *fs, int verbose) {
    Metrics m = {0, 0, 0, 0};
    int nfile = 0, ndir = 0, nchild = 0, same = 0, pdsum = 0;
    for (int k = 0; k < fs->nfiles; k++) {
        File *f = &fs->files[k];
        if (!f->alive) continue;
        if (!f->isdir) { int lo = 1 << 30, hi = -1; span_add(fs, f, &lo, &hi); m.avgfile += hi - lo; nfile++; }
        if (f->parent[0]) {
            int pg = fs->files[find(fs, f->parent)].igroup, d = abs(f->igroup - pg);
            nchild++; pdsum += d; same += d == 0;
        }
        if (f->isdir) {
            int lo = 1 << 30, hi = -1, kids = 0;
            span_add(fs, f, &lo, &hi);
            for (int c = 0; c < fs->nfiles; c++)
                if (fs->files[c].alive && !strcmp(fs->files[c].parent, f->path)) { span_add(fs, &fs->files[c], &lo, &hi); kids++; }
            if (kids) {
                if (verbose) printf("  dirspan(%s) = %d\n", f->path, hi - lo);
                m.avgdir += hi - lo; ndir++;
            }
        }
    }
    if (ndir) m.avgdir /= ndir;
    if (nfile) m.avgfile /= nfile;
    if (nchild) { m.same = 100.0 * same / nchild; m.avgpd = (double)pdsum / nchild; }
    return m;
}

int main(void) {
    const char *pol[] = {"ffs", "spread", "random"};
    printf("=== 示例 1：原书 /a /b 例子（8 组，每组 10 inode + 10 数据块）===\n");
    for (int p = 0; p < 3; p++) {
        static FS fs;
        fs_init(&fs, 8, 10, 10, 0, 1, pol[p], 1);
        create(&fs, "/", 1, 0);
        create(&fs, "/a", 1, 0); create(&fs, "/b", 1, 0);
        create(&fs, "/a/c", 0, 2); create(&fs, "/a/d", 0, 2); create(&fs, "/a/e", 0, 2); create(&fs, "/b/f", 0, 2);
        printf("\n[%s]\n", pol[p]);
        print_map(&fs);
        metrics(&fs, 1);
    }

    printf("\n=== 示例 2：30 块的大文件 /a（每组 10 inode + 40 数据块）===\n");
    int Ls[] = {0, 5};
    for (int k = 0; k < 2; k++) {
        static FS fs;
        fs_init(&fs, 7, 10, 40, Ls[k], 1, "ffs", 1);
        create(&fs, "/", 1, 0); create(&fs, "/a", 0, 30);
        printf("\n[大文件例外 L=%d%s]\n", Ls[k], Ls[k] ? " 块/chunk" : "（关闭）");
        print_map(&fs);
    }

    printf("\n=== 示例 3：40 块文件的 filespan 随大文件例外参数 L 的变化（10 组 × 40 数据块）===\n");
    int Ls2[] = {0, 4, 10, 30, 100};
    for (int k = 0; k < 5; k++) {
        static FS fs;
        fs_init(&fs, 10, 10, 40, Ls2[k], 1, "ffs", 1);
        create(&fs, "/", 1, 0); create(&fs, "/a", 0, 40);
        int lo = 1 << 30, hi = -1; span_add(&fs, &fs.files[1], &lo, &hi);
        printf("  L=%-3d filespan(/a)=%d\n", Ls2[k], hi - lo);
    }

    printf("\n=== 示例 4：6 个目录 × 20 个文件（交错创建，大小 1~6 块，种子 42），16 组 × (32 inode + 128 数据块) ===\n");
    printf("%-8s %12s %12s %12s %14s\n", "policy", "avg_dirspan", "avg_filespan", "same_group", "avg_group_dist");
    for (int p = 0; p < 3; p++) {
        static FS fs;
        fs_init(&fs, 16, 32, 128, 0, 1, pol[p], 7);
        create(&fs, "/", 1, 0);
        char path[64];
        for (int d = 0; d < 6; d++) { snprintf(path, sizeof path, "/d%d", d); create(&fs, path, 1, 0); }
        wl_state = 42;
        for (int f = 0; f < 20; f++)
            for (int d = 0; d < 6; d++) {
                snprintf(path, sizeof path, "/d%d/f%d", d, f);
                create(&fs, path, 0, 1 + (int)(wl_rnd() * 6));
            }
        Metrics m = metrics(&fs, 0);
        printf("%-8s %12.2f %12.2f %11.1f%% %14.2f\n", pol[p], m.avgdir, m.avgfile, m.same, m.avgpd);
    }

    printf("\n=== 分摊：定位 10ms、带宽 40MB/s 时，达到峰值带宽 p 所需的 chunk 大小 ===\n");
    double ps[] = {0.5, 0.9, 0.99};
    for (int k = 0; k < 3; k++) {
        double kb = 40 * 1024 * (10 / 1000.0) * ps[k] / (1 - ps[k]);
        printf("  %4.0f%%  chunk = %9.1f KB = %6.2f MB\n", ps[k] * 100, kb, kb / 1024);
    }
    return 0;
}
