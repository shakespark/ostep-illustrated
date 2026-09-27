// workload.c —— 复刻 OSTEP Figure 22.6–22.8：三种负载下各策略命中率随缓存大小的变化
// 每种负载 10,000 次访问；无局部性/80-20 为 100 个页，循环顺序为 50 个页。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define N 10000
static int refs[N];
static unsigned rs;
static double rnd(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs / 4294967296.0; }

static int in(int *m, int n, int p) { for (int i = 0; i < n; i++) if (m[i] == p) return i; return -1; }

static int run(int pol, int size) { // 0=OPT 1=LRU 2=FIFO 3=RAND 4=CLOCK
    int mem[128], use[128] = {0}, n = 0, hand = 0, hits = 0;
    for (int i = 0; i < N; i++) {
        int p = refs[i], at = in(mem, n, p);
        if (at >= 0) {
            hits++;
            if (pol == 1) { memmove(&mem[at], &mem[at + 1], (n - at - 1) * sizeof(int)); mem[n - 1] = p; }
            if (pol == 4) use[at] = 1;
            continue;
        }
        if (n < size) { mem[n] = p; use[n] = 1; n++; continue; }
        int idx = 0;
        if (pol == 4) {
            while (use[hand]) { use[hand] = 0; hand = (hand + 1) % size; }
            mem[hand] = p; use[hand] = 1; hand = (hand + 1) % size; continue;
        }
        if (pol == 3) idx = (int)(rnd() * n);
        if (pol == 0) {
            int best = -1;
            for (int k = 0; k < n; k++) {
                int when = N + 1;
                for (int j = i + 1; j < N; j++) if (refs[j] == mem[k]) { when = j; break; }
                if (when > best) { best = when; idx = k; }
            }
        }
        memmove(&mem[idx], &mem[idx + 1], (n - idx - 1) * sizeof(int));
        mem[n - 1] = p;
    }
    return hits;
}

int main(void) {
    const char *names[] = { "no-locality", "80-20", "looping" };
    const int sizes[] = { 1, 10, 20, 30, 40, 49, 50, 60, 80, 100 };
    for (int w = 0; w < 3; w++) {
        rs = 12345;
        for (int i = 0; i < N; i++) {
            if (w == 0) refs[i] = (int)(rnd() * 100);
            else if (w == 1) refs[i] = rnd() < 0.8 ? (int)(rnd() * 20) : 20 + (int)(rnd() * 80);
            else refs[i] = i % 50;
        }
        printf("== %s workload ==\ncache   OPT    LRU   FIFO   RAND  CLOCK\n", names[w]);
        for (unsigned k = 0; k < sizeof sizes / sizeof sizes[0]; k++) {
            printf("%5d", sizes[k]);
            for (int pol = 0; pol < 5; pol++) printf(" %5.1f%%", 100.0 * run(pol, sizes[k]) / N);
            printf("\n");
        }
    }
    return 0;
}
