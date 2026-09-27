// csum.c —— 第 45 章实验：XOR / 加法 / Fletcher / CRC32 四种校验和
// 1) 自检：原书 16 字节示例的 XOR 结果应为 0x201b9403；CRC32("123456789") 应为 0xCBF43926
// 2) 测速：对一块 64 MB 的缓冲区反复计算，报告 MB/s
// 3) 检测率：对 4 KB 随机块做随机单比特/双比特翻转以及几种"结构化"损坏，统计各校验和能否发现
#define _POSIX_C_SOURCE 199309L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---------- 校验和实现 ----------

// XOR：把数据看成一串 32 位字（大端，与原书按"4 字节一行"对齐的写法一致），逐列异或
static uint32_t xor32(const uint8_t *p, size_t n) {
    uint32_t c = 0;
    for (size_t i = 0; i + 4 <= n; i += 4)
        c ^= (uint32_t)p[i] << 24 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 8 | p[i + 3];
    return c;
}
// XOR 的快速版：一次处理 8 字节，最后折叠成 32 位（数值与 xor32 在小端机上不同，只用于测速）
static uint32_t xor64_fast(const uint8_t *p, size_t n) {
    uint64_t c = 0, w;
    for (size_t i = 0; i + 8 <= n; i += 8) { memcpy(&w, p + i, 8); c ^= w; }
    return (uint32_t)(c ^ (c >> 32));
}
// 加法：32 位字的二进制补码加法，忽略溢出
static uint32_t add32(const uint8_t *p, size_t n) {
    uint32_t c = 0;
    for (size_t i = 0; i + 4 <= n; i += 4)
        c += (uint32_t)p[i] << 24 | (uint32_t)p[i + 1] << 16 | (uint32_t)p[i + 2] << 8 | p[i + 3];
    return c;
}
// Fletcher（原书定义）：s1 = (s1 + d_i) mod 255；s2 = (s2 + s1) mod 255，每个字节都取模
static uint32_t fletcher_naive(const uint8_t *p, size_t n) {
    uint32_t s1 = 0, s2 = 0;
    for (size_t i = 0; i < n; i++) { s1 = (s1 + p[i]) % 255; s2 = (s2 + s1) % 255; }
    return s2 << 8 | s1;
}
// Fletcher 快速版：结果完全相同，但把取模推迟到每 4096 字节做一次（64 位累加器不会溢出）
static uint32_t fletcher_fast(const uint8_t *p, size_t n) {
    uint64_t s1 = 0, s2 = 0;
    while (n) {
        size_t k = n < 4096 ? n : 4096;
        for (size_t i = 0; i < k; i++) { s1 += p[i]; s2 += s1; }
        s1 %= 255; s2 %= 255; p += k; n -= k;
    }
    return (uint32_t)(s2 << 8 | s1);
}
// CRC32（IEEE 802.3，反射多项式 0xEDB88320）逐位版：就是一位一位做"模 2 长除法"
static uint32_t crc32_bitwise(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}
// CRC32 查表版：预先算好 256 项，一次处理一个字节
static uint32_t T[256];
static void crc32_init(void) {
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1)));
        T[i] = c;
    }
}
static uint32_t crc32_table(const uint8_t *p, size_t n) {
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) c = T[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

typedef uint32_t (*csum_fn)(const uint8_t *, size_t);
struct algo { const char *name; csum_fn f; };

// ---------- 小工具 ----------
static uint64_t rs = 88172645463325252ull;  // xorshift64，可复现
static uint64_t rnd(void) { rs ^= rs << 13; rs ^= rs >> 7; rs ^= rs << 17; return rs; }
static double now(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec + t.tv_nsec / 1e9; }
static void flip(uint8_t *b, size_t bit) { b[bit / 8] ^= (uint8_t)(0x80 >> (bit % 8)); }

int main(int argc, char **argv) {
    crc32_init();
    int trials = argc > 1 ? atoi(argv[1]) : 100000;

    // ---- 1. 自检 ----
    const uint8_t book[16] = {0x36,0x5e,0xc4,0xcd,0xba,0x14,0x8a,0x92,0xec,0xef,0x2c,0x3a,0x40,0xbe,0xf6,0x66};
    const char *tv = "123456789";
    printf("== 自检 ==\n");
    printf("原书示例 XOR      = 0x%08x （原书：0x201b9403）%s\n", xor32(book, 16), xor32(book, 16) == 0x201b9403u ? " OK" : " 错误!");
    printf("原书示例 加法     = 0x%08x\n", add32(book, 16));
    printf("原书示例 Fletcher = s2=0x%02x s1=0x%02x\n", fletcher_naive(book, 16) >> 8, fletcher_naive(book, 16) & 0xff);
    printf("原书示例 CRC32    = 0x%08x\n", crc32_table(book, 16));
    uint32_t c1 = crc32_bitwise((const uint8_t *)tv, 9), c2 = crc32_table((const uint8_t *)tv, 9);
    printf("CRC32(\"123456789\") 逐位=0x%08x 查表=0x%08x （标准值 0xcbf43926）%s\n", c1, c2,
           c1 == 0xCBF43926u && c2 == 0xCBF43926u ? " OK" : " 错误!");

    // ---- 2. 测速 ----
    const size_t N = 64u << 20;
    uint8_t *buf = malloc(N);
    for (size_t i = 0; i < N; i += 8) { uint64_t r = rnd(); memcpy(buf + i, &r, 8); }
    if (fletcher_naive(buf, 1 << 20) != fletcher_fast(buf, 1 << 20)) { printf("Fletcher 两种实现不一致!\n"); return 1; }
    struct algo speed[] = {
        {"XOR (64位字)", xor64_fast}, {"XOR (32位字,大端)", xor32}, {"加法 (32位字)", add32},
        {"Fletcher 朴素", fletcher_naive}, {"Fletcher 推迟取模", fletcher_fast},
        {"CRC32 逐位", crc32_bitwise}, {"CRC32 查表", crc32_table},
    };
    printf("\n== 测速：64 MB 随机数据（每种算至少 0.3 秒）==\n");
    volatile uint32_t sink = 0;
    for (size_t a = 0; a < sizeof speed / sizeof speed[0]; a++) {
        double t0 = now(), t; int reps = 0;
        do { sink ^= speed[a].f(buf, N); reps++; t = now() - t0; } while (t < 0.3);
        printf("%9.1f MB/s   %s\n", reps * (N / 1048576.0) / t, speed[a].name);
    }

    // ---- 3. 检测率 ----
    struct algo det[] = {{"XOR", xor32}, {"加法", add32}, {"Fletcher", fletcher_fast}, {"CRC32", crc32_table}};
    const int NA = 4; const size_t B = 4096, BITS = B * 8;
    uint8_t blk[4096], bad[4096];
    const char *kinds[] = {"随机 1 比特翻转", "随机 2 比特翻转", "同列 2 比特翻转(相隔 4 字节倍数)", "交换两个 4 字节字", "随机 3 比特翻转"};
    printf("\n== 检测率：4 KB 块，每种损坏 %d 次 ==\n", trials);
    printf("       XOR      加法  Fletcher     CRC32   损坏方式\n");  // 手工对齐（中文占两列）
    for (int k = 0; k < 5; k++) {
        long miss[4] = {0};
        for (int t = 0; t < trials; t++) {
            for (size_t i = 0; i < B; i += 8) { uint64_t r = rnd(); memcpy(blk + i, &r, 8); }
            memcpy(bad, blk, B);
            if (k == 0) flip(bad, rnd() % BITS);
            else if (k == 1 || k == 4) {
                int nb = k == 1 ? 2 : 3; size_t used[3];
                for (int j = 0; j < nb; j++) {
                    size_t b; int dup;
                    do { b = rnd() % BITS; dup = 0; for (int q = 0; q < j; q++) dup |= used[q] == b; } while (dup);
                    used[j] = b; flip(bad, b);
                }
            } else if (k == 2) {
                size_t w1 = rnd() % (B / 4), w2; do w2 = rnd() % (B / 4); while (w2 == w1);
                size_t col = rnd() % 32;
                flip(bad, w1 * 32 + col); flip(bad, w2 * 32 + col);
            } else {
                size_t w1 = rnd() % (B / 4), w2; do w2 = rnd() % (B / 4); while (w2 == w1);
                if (!memcmp(blk + w1 * 4, blk + w2 * 4, 4)) { t--; continue; }
                uint8_t tmp[4]; memcpy(tmp, bad + w1 * 4, 4); memcpy(bad + w1 * 4, bad + w2 * 4, 4); memcpy(bad + w2 * 4, tmp, 4);
            }
            for (int a = 0; a < NA; a++) if (det[a].f(blk, B) == det[a].f(bad, B)) miss[a]++;
        }
        for (int a = 0; a < NA; a++) printf("%9.3f%%", 100.0 * (trials - miss[a]) / trials);
        printf("   %s\n", kinds[k]);
    }
    free(buf);
    return (int)(sink & 0);
}
