// paging-policy.c —— 页替换策略模拟器（仿 OSTEP 官方 paging-policy.py）
// 用法：
//   ./paging-policy -p FIFO|LRU|OPT|CLOCK|RAND|ALL -c 缓存大小 -a 0,1,2,...   [-v] [-s 种子]
//   ./paging-policy -p LRU -c 3 -n 10 -m 10 -s 0     随机生成 10 个访问（页号 0..9）
//   ./paging-policy -p LRU -c 64 -f trace.txt        从文件读取页号（空白/逗号分隔）
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXREF 1000000

static int refs[MAXREF], nref = 0;
static int verbose = 0;

static void add_ref(int p) { if (nref < MAXREF) refs[nref++] = p; }

static void parse_list(const char *s) {
    char *buf = strdup(s), *save = NULL;
    for (char *t = strtok_r(buf, ", \t\n", &save); t; t = strtok_r(NULL, ", \t\n", &save)) add_ref(atoi(t));
    free(buf);
}

static void read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); exit(1); }
    int p;
    while (fscanf(f, " %d%*[, ]", &p) == 1) add_ref(p);
    fclose(f);
}

// 简单可复现随机数（xorshift32）
static unsigned rs = 1;
static unsigned xr(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }

static int find(int *mem, int n, int p) { for (int i = 0; i < n; i++) if (mem[i] == p) return i; return -1; }

// 返回命中次数；cold 返回冷启动未命中次数
static int simulate(const char *pol, int size, int *cold_out) {
    int *mem = calloc(size, sizeof(int)), *use = calloc(size, sizeof(int));
    int n = 0, hand = 0, hits = 0, misses = 0, cold = 0;
    char *seen = calloc(1 << 20, 1);
    int clock = strcmp(pol, "CLOCK") == 0;
    for (int i = 0; i < nref; i++) {
        int p = refs[i], at = find(mem, n, p), victim = -1;
        int first = !seen[p & ((1 << 20) - 1)];
        seen[p & ((1 << 20) - 1)] = 1;
        if (at >= 0) {
            hits++;
            if (!strcmp(pol, "LRU")) { memmove(&mem[at], &mem[at + 1], (n - at - 1) * sizeof(int)); mem[n - 1] = p; }
            if (clock) use[at] = 1;
        } else {
            misses++; if (first) cold++;
            if (clock) {
                if (n < size) { mem[n] = p; use[n] = 1; n++; }
                else {
                    while (use[hand]) { use[hand] = 0; hand = (hand + 1) % size; }
                    victim = mem[hand]; mem[hand] = p; use[hand] = 1; hand = (hand + 1) % size;
                }
            } else {
                if (n == size) {
                    int idx = 0;
                    if (!strcmp(pol, "RAND")) idx = xr() % n;
                    else if (!strcmp(pol, "OPT")) {
                        int best = -1;
                        for (int k = 0; k < n; k++) {
                            int when = nref;
                            for (int j = i + 1; j < nref; j++) if (refs[j] == mem[k]) { when = j; break; }
                            if (when >= best) { best = when; idx = k; }   // 平局取靠后者，与 paging-policy.py 一致
                        }
                    }
                    victim = mem[idx];
                    memmove(&mem[idx], &mem[idx + 1], (n - idx - 1) * sizeof(int));
                    n--;
                }
                mem[n++] = p;
            }
        }
        if (verbose) {
            printf("Access: %d  %-4s  ", p, at >= 0 ? "HIT" : "MISS");
            if (!strcmp(pol, "FIFO")) printf("FirstIn -> ");
            else if (!strcmp(pol, "LRU")) printf("LRU -> ");
            printf("[");
            for (int k = 0; k < n; k++) printf(k ? " %d" : "%d", mem[k]);
            printf("]");
            if (!strcmp(pol, "FIFO")) printf(" <- LastIn");
            else if (!strcmp(pol, "LRU")) printf(" <- MRU");
            if (clock) { printf(" use="); for (int k = 0; k < n; k++) printf("%d", use[k]); printf(" hand=%d", hand); }
            if (victim >= 0) printf("  Replaced:%d", victim); else printf("  Replaced:-");
            printf("  [Hits:%d Misses:%d]\n", hits, misses);
        }
    }
    free(mem); free(use); free(seen);
    if (cold_out) *cold_out = cold;
    return hits;
}

int main(int argc, char *argv[]) {
    const char *pol = "FIFO";
    int size = 3, gen_n = 0, gen_m = 10, seed = 0, opt;
    while ((opt = getopt(argc, argv, "p:c:a:f:n:m:s:v")) != -1) {
        switch (opt) {
        case 'p': pol = optarg; break;
        case 'c': size = atoi(optarg); break;
        case 'a': parse_list(optarg); break;
        case 'f': read_file(optarg); break;
        case 'n': gen_n = atoi(optarg); break;
        case 'm': gen_m = atoi(optarg); break;
        case 's': seed = atoi(optarg); break;
        case 'v': verbose = 1; break;
        default: fprintf(stderr, "usage: %s -p POLICY -c SIZE (-a LIST | -f FILE | -n N -m MAXPAGE) [-s SEED] [-v]\n", argv[0]); return 1;
        }
    }
    rs = (unsigned)seed * 2654435761u + 1u;
    for (int i = 0; i < gen_n; i++) add_ref(xr() % gen_m);
    if (nref == 0) { fprintf(stderr, "no references given\n"); return 1; }
    if (size < 1) size = 1;
    if (!strcmp(pol, "ALL")) {
        const char *all[] = { "OPT", "FIFO", "LRU", "CLOCK" };
        printf("policy  hits  misses  hitrate  hitrate(excl. cold)\n");
        for (int k = 0; k < 4; k++) {
            int cold, h = simulate(all[k], size, &cold);
            printf("%-6s %5d  %6d   %5.1f%%   %5.1f%%\n", all[k], h, nref - h, 100.0 * h / nref,
                   nref - cold ? 100.0 * h / (nref - cold) : 0.0);
        }
        return 0;
    }
    int cold, h = simulate(pol, size, &cold);
    printf("FINALSTATS hits %d   misses %d   hitrate %.2f\n", h, nref - h, 100.0 * h / nref);
    return 0;
}
