// raid.c —— 用内存里的数组模拟 N 块磁盘，实现 RAID-5（左对称）的条带化与 XOR 校验
//   第 1 部分：写入数据 → 拔掉一块盘 → 用 XOR 重建 → 与原始数据逐字节比较
//   第 2 部分：小写（small write）的两种校验更新方法：加法 additive vs 减法 subtractive
//             验证两者算出的校验块完全相同，并统计各自需要多少次物理 I/O
// 用法：./raid [磁盘数N] [块大小字节]      默认 N=5，块大小 4096
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int N = 5, BS = 4096, ROWS = 64;   // 每块盘 ROWS 个块
static unsigned char ***disk;              // disk[d][row] -> BS 字节
static long reads, writes;                 // 物理 I/O 计数

static void rd(int d, int r, unsigned char *buf) { memcpy(buf, disk[d][r], BS); reads++; }
static void wr(int d, int r, const unsigned char *buf) { memcpy(disk[d][r], buf, BS); writes++; }
static void xor_into(unsigned char *dst, const unsigned char *src) { for (int i = 0; i < BS; i++) dst[i] ^= src[i]; }

// 逻辑块 A → (磁盘, 行)，以及该行的校验盘。与原书 Figure 38.7 相同的左对称布局
static int pdisk_of(int row) { return N - 1 - row % N; }
static void map(int A, int *d, int *row) {
    *row = A / (N - 1);
    int col = A % (N - 1), p = pdisk_of(*row);
    *d = (p + 1 + col) % N;
}

static void alloc_disks(void) {
    disk = malloc(N * sizeof *disk);
    for (int d = 0; d < N; d++) {
        disk[d] = malloc(ROWS * sizeof **disk);
        for (int r = 0; r < ROWS; r++) disk[d][r] = calloc(1, BS);
    }
}
static void free_disks(void) {
    for (int d = 0; d < N; d++) { for (int r = 0; r < ROWS; r++) free(disk[d][r]); free(disk[d]); }
    free(disk);
}

// 全条带写：一次写满一行的 N-1 个数据块，校验直接由新数据算出，不需要任何读
static void full_stripe_write(int row, unsigned char **data) {
    unsigned char *p = calloc(1, BS);
    for (int col = 0; col < N - 1; col++) {        // 第 col 个数据块放在校验盘右边第 col+1 块盘上
        int d = (pdisk_of(row) + 1 + col) % N;
        wr(d, row, data[col]); xor_into(p, data[col]);
    }
    wr(pdisk_of(row), row, p);
    free(p);
}

// 小写：更新逻辑块 A。method 0 = 减法，1 = 加法
static void small_write(int A, const unsigned char *newdata, int method) {
    int d, row; map(A, &d, &row);
    int p = pdisk_of(row);
    unsigned char *par = malloc(BS), *tmp = malloc(BS);
    if (method == 0) {                 // P_new = (C_old ⊕ C_new) ⊕ P_old
        rd(d, row, tmp); rd(p, row, par);
        xor_into(par, tmp); xor_into(par, newdata);
    } else {                           // P_new = 新数据 ⊕ 同行其他所有数据块
        memcpy(par, newdata, BS);
        for (int e = 0; e < N; e++) if (e != d && e != p) { rd(e, row, tmp); xor_into(par, tmp); }
    }
    wr(d, row, newdata); wr(p, row, par);
    free(par); free(tmp);
}

// 从其余 N-1 块盘重建磁盘 bad 的第 row 块
static void reconstruct(int bad, int row, unsigned char *out) {
    unsigned char *tmp = malloc(BS);
    memset(out, 0, BS);
    for (int e = 0; e < N; e++) if (e != bad) { rd(e, row, tmp); xor_into(out, tmp); }
    free(tmp);
}

static void fill(unsigned char *b, unsigned seed) { for (int i = 0; i < BS; i++) { seed = seed * 1103515245u + 12345u; b[i] = seed >> 16; } }

int main(int argc, char *argv[]) {
    if (argc > 1) N = atoi(argv[1]);
    if (argc > 2) BS = atoi(argv[2]);
    if (N < 3 || BS < 1) { fprintf(stderr, "需要 N>=3\n"); return 1; }
    alloc_disks();
    int nblocks = ROWS * (N - 1);
    printf("== RAID-5（左对称），%d 块盘，每块 %d 字节，每盘 %d 块，可用 %d 个逻辑块 ==\n", N, BS, ROWS, nblocks);
    printf("前 3 行布局（与原书 Figure 38.7 相同的规则）：\n");
    for (int r = 0; r < 3; r++) {
        printf("  ");
        for (int d = 0; d < N; d++) {
            if (d == pdisk_of(r)) { printf("  P%-3d", r); continue; }
            for (int A = r * (N - 1); A < (r + 1) * (N - 1); A++) { int dd, rr; map(A, &dd, &rr); if (dd == d) printf("%5d ", A); }
        }
        printf("\n");
    }

    // 1. 用全条带写填满整个阵列，同时保留一份"真相"用于比对
    unsigned char **truth = malloc(nblocks * sizeof *truth);
    for (int A = 0; A < nblocks; A++) { truth[A] = malloc(BS); fill(truth[A], 1000 + A); }
    reads = writes = 0;
    for (int r = 0; r < ROWS; r++) full_stripe_write(r, &truth[r * (N - 1)]);
    printf("\n[1] 全条带写填满阵列：%ld 次读，%ld 次写（每行 %d 写、0 读）\n", reads, writes, N);

    // 2. 拔掉磁盘 2，逐块重建，并与真相比较
    int bad = 2;
    unsigned char *rebuilt = malloc(BS);
    reads = writes = 0;
    int ok = 0, lostdata = 0;
    for (int r = 0; r < ROWS; r++) {
        reconstruct(bad, r, rebuilt);
        if (bad == pdisk_of(r)) {                // 这一行丢的是校验块：与重新计算的校验比较
            if (memcmp(rebuilt, disk[bad][r], BS) == 0) ok++;
            continue;
        }
        lostdata++;
        for (int A = r * (N - 1); A < (r + 1) * (N - 1); A++) {
            int d, rr; map(A, &d, &rr);
            if (d == bad) { if (memcmp(rebuilt, truth[A], BS) == 0) ok++; else printf("  块 %d 重建错误!\n", A); }
        }
    }
    printf("[2] 磁盘 %d 故障 → 重建它的 %d 个块（%d 个数据块 + %d 个校验块）：%d/%d 与原数据完全一致，共 %ld 次读\n",
           bad, ROWS, lostdata, ROWS - lostdata, ok, ROWS, reads);
    {   // 展示第 0 行重建的第一个字节
        int r = 0; unsigned char x = 0;
        printf("    例：第 0 行第 0 字节：");
        for (int e = 0; e < N; e++) if (e != bad) { printf("%s0x%02X", x || e ? " ⊕ " : "", disk[e][r][0]); x ^= disk[e][r][0]; }
        printf(" = 0x%02X（盘 %d 上原本是 0x%02X）\n", x, bad, disk[bad][r][0]);
    }

    // 3. 小写：同一组随机写分别用减法 / 加法做，比较 I/O 数，并验证校验块一致
    printf("\n[3] 1000 次随机单块小写：加法 vs 减法（N 从 3 到 8）\n");
    printf("    N | 加法 I/O/次 | 减法 I/O/次 | 两种方法的校验块是否相同 | 更省的方法\n");
    free_disks();
    int saveN = N;
    for (N = 3; N <= 8; N++) {
        long io[2]; unsigned char *finalp[2][4];
        for (int m = 0; m < 2; m++) {
            alloc_disks();
            nblocks = ROWS * (N - 1);
            unsigned char **init = malloc((size_t)nblocks * sizeof *init);
            for (int A = 0; A < nblocks; A++) { init[A] = malloc(BS); fill(init[A], 7 * A + 1); }
            for (int r = 0; r < ROWS; r++) full_stripe_write(r, &init[r * (N - 1)]);
            reads = writes = 0;
            srand(42);                          // 两种方法用完全相同的写序列
            unsigned char *nd = malloc(BS);
            for (int i = 0; i < 1000; i++) { int A = rand() % nblocks; fill(nd, rand()); small_write(A, nd, m); }
            io[m] = reads + writes;
            for (int r = 0; r < 4; r++) { finalp[m][r] = malloc(BS); memcpy(finalp[m][r], disk[pdisk_of(r)][r], BS); }
            free(nd);
            for (int A = 0; A < nblocks; A++) free(init[A]);
            free(init);
            free_disks();
        }
        int same = 1;
        for (int r = 0; r < 4; r++) { if (memcmp(finalp[0][r], finalp[1][r], BS)) same = 0; free(finalp[0][r]); free(finalp[1][r]); }
        double a = io[1] / 1000.0, s = io[0] / 1000.0;
        printf("    %d | %11.1f | %11.1f | %-24s | %s\n", N, a, s, same ? "相同 ✓" : "不同 ✗", a < s ? "加法" : a > s ? "减法" : "一样多");
    }
    N = saveN;
    for (int A = 0; A < ROWS * (N - 1); A++) free(truth[A]);
    free(truth); free(rebuilt);
    return 0;
}
