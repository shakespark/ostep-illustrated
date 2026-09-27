// 第 42 章实验：在一个"磁盘镜像文件"上实现原书的追加例子（I[v2]、B[v2]、Db），
// 分别用 无日志 / 数据日志 / 无屏障的数据日志 / 有序元数据日志 四种协议写入，
// 并在"第 n 个块写落盘之后"注入崩溃（子进程 _exit），再运行恢复（replay）和一致性检查。
//
// 用法：
//   ./journal crashtest           穷举所有模式 × 所有崩溃点，打印恢复后的文件系统状态
//   ./journal show MODE N         只跑一个模式、在 N 个写之后崩溃，打印磁盘镜像细节
//   ./journal fsyncbench          测量 write / write+fsync / write+fdatasync / 原子 rename 的真实开销
//
// 磁盘布局（每块 512 字节，512 字节写入被视为原子）：
//   0: 日志超级块 JSB   1..5: 日志区   6: 数据位图 B   7: inode I   8..15: 数据块 D0..D7
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <time.h>

#define BS 512
#define NBLK 16
#define JSB 0
#define JLOG 1
#define BMAP 6
#define INODE 7
#define DATA0 8
#define IMG "disk.img"

static int fd;
static int crash_after = -1;   // -1 表示不崩溃
static int nwrites = 0;
static const char *wname[32];  // 本次已落盘的写的名字（仅用于打印）

// ---------- 块读写 ----------
static void bread(int b, char *buf) {
    if (pread(fd, buf, BS, (off_t)b * BS) != BS) { perror("pread"); exit(1); }
}
// 每次块写都经过这里：写满 crash_after 个之后，"电源被拔掉"
static void bwrite(int b, const char *buf, const char *name) {
    if (crash_after >= 0 && nwrites >= crash_after) _exit(42);
    if (pwrite(fd, buf, BS, (off_t)b * BS) != BS) { perror("pwrite"); exit(1); }
    wname[nwrites++] = name;
}
// 写屏障：真实系统里这里是 FLUSH/FUA；本实验中块写本来就按顺序落盘，fsync 只是让语义完整
static void barrier(void) { fsync(fd); }

static void fill(char *buf, const char *s) { memset(buf, 0, BS); snprintf(buf, BS, "%s", s); }

// ---------- mkfs：初始状态 I[v1]、B[v1]、Da ----------
static void mkfs(void) {
    char buf[BS];
    fd = open(IMG, O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { perror("open"); exit(1); }
    for (int b = 0; b < NBLK; b++) {
        char g[64]; snprintf(g, sizeof g, "GARBAGE(old block %d)", b);
        fill(buf, g);
        if (pwrite(fd, buf, BS, (off_t)b * BS) != BS) { perror("pwrite"); exit(1); }
    }
    fill(buf, "JSB freed=0");               bwrite(JSB, buf, "init");
    fill(buf, "00001000");                  bwrite(BMAP, buf, "init");
    fill(buf, "INODE size=1 ptr=4,-1,-1,-1"); bwrite(INODE, buf, "init");
    fill(buf, "Da: first block of file");   bwrite(DATA0 + 4, buf, "init");
    fsync(fd);
    nwrites = 0;
}

// 新版本的三个块
static char Iv2[BS], Bv2[BS], Db[BS];
static void prepare(void) {
    fill(Iv2, "INODE size=2 ptr=4,5,-1,-1");
    fill(Bv2, "00001100");
    fill(Db, "Db: appended user data");
}

// 日志事务：TxB 里记录事务号和每个日志块的最终地址
static void txb(char *buf, int n, const int *addrs) {
    char s[128]; int k = snprintf(s, sizeof s, "TXB tid=1 n=%d addrs=", n);
    for (int i = 0; i < n; i++) k += snprintf(s + k, sizeof s - k, "%d%s", addrs[i], i + 1 < n ? "," : "");
    fill(buf, s);
}

// ---------- 四种写协议 ----------
static void append(const char *mode) {
    char buf[BS];
    prepare();
    if (!strcmp(mode, "none")) {                 // 直接原地写，顺序 I、B、Db
        bwrite(INODE, Iv2, "I[v2]"); bwrite(BMAP, Bv2, "B[v2]"); bwrite(DATA0 + 5, Db, "Db");
    } else if (!strcmp(mode, "data")) {          // 数据日志（ext3 data 模式）
        int a[3] = { INODE, BMAP, DATA0 + 5 };
        txb(buf, 3, a);         bwrite(JLOG + 0, buf, "J:TxB");
        bwrite(JLOG + 1, Iv2, "J:I[v2]"); bwrite(JLOG + 2, Bv2, "J:B[v2]"); bwrite(JLOG + 3, Db, "J:Db");
        barrier();                                                     // 1. Journal write 完成
        fill(buf, "TXE tid=1"); bwrite(JLOG + 4, buf, "J:TxE");
        barrier();                                                     // 2. Journal commit 完成
        bwrite(INODE, Iv2, "I[v2]"); bwrite(BMAP, Bv2, "B[v2]"); bwrite(DATA0 + 5, Db, "Db");
        barrier();                                                     // 3. Checkpoint 完成
        fill(buf, "JSB freed=1"); bwrite(JSB, buf, "JSB:free");        // 4. Free
    } else if (!strcmp(mode, "nobarrier")) {     // 五个日志块一次性发出，磁盘内部重排：Db 最后才落盘
        int a[3] = { INODE, BMAP, DATA0 + 5 };
        txb(buf, 3, a);         bwrite(JLOG + 0, buf, "J:TxB");
        bwrite(JLOG + 1, Iv2, "J:I[v2]"); bwrite(JLOG + 2, Bv2, "J:B[v2]");
        fill(buf, "TXE tid=1"); bwrite(JLOG + 4, buf, "J:TxE");         // TxE 抢在 Db 前面落盘
        bwrite(JLOG + 3, Db, "J:Db");
        barrier();
        bwrite(INODE, Iv2, "I[v2]"); bwrite(BMAP, Bv2, "B[v2]"); bwrite(DATA0 + 5, Db, "Db");
        barrier();
        fill(buf, "JSB freed=1"); bwrite(JSB, buf, "JSB:free");
    } else if (!strcmp(mode, "ordered")) {       // 有序元数据日志（ext3 ordered 模式）：Db 先写到最终位置
        int a[2] = { INODE, BMAP };
        bwrite(DATA0 + 5, Db, "Db");                                   // 1. Data write
        txb(buf, 2, a);         bwrite(JLOG + 0, buf, "J:TxB");        // 2. Journal metadata write
        bwrite(JLOG + 1, Iv2, "J:I[v2]"); bwrite(JLOG + 2, Bv2, "J:B[v2]");
        barrier();
        fill(buf, "TXE tid=1"); bwrite(JLOG + 3, buf, "J:TxE");         // 3. Journal commit
        barrier();
        bwrite(INODE, Iv2, "I[v2]"); bwrite(BMAP, Bv2, "B[v2]");         // 4. Checkpoint metadata
        barrier();
        fill(buf, "JSB freed=1"); bwrite(JSB, buf, "JSB:free");        // 5. Free
    } else { fprintf(stderr, "unknown mode %s\n", mode); exit(2); }
    fsync(fd);
}

// ---------- 恢复：扫描日志，重放已提交且未释放的事务（redo logging） ----------
static const char *recover(void) {
    char jsb[BS], b0[BS], be[BS], blk[BS];
    bread(JSB, jsb); bread(JLOG, b0);
    int freed = 0; sscanf(jsb, "JSB freed=%d", &freed);
    int tid, n; char addrs[64];
    if (sscanf(b0, "TXB tid=%d n=%d addrs=%63s", &tid, &n, addrs) != 3) return "日志中没有事务 → 什么都不做";
    if (tid <= freed) return "事务已 checkpoint 并释放 → 跳过";
    bread(JLOG + 1 + n, be);
    int etid;
    if (sscanf(be, "TXE tid=%d", &etid) != 1 || etid != tid) return "有 TxB 无匹配 TxE（未提交）→ 丢弃事务";
    int a[8], k = 0; char *p = addrs;
    while (k < n && sscanf(p, "%d", &a[k]) == 1) { k++; p = strchr(p, ','); if (!p) break; p++; }
    for (int i = 0; i < n; i++) { bread(JLOG + 1 + i, blk); pwrite(fd, blk, BS, (off_t)a[i] * BS); }
    fill(blk, "JSB freed=1"); pwrite(fd, blk, BS, (off_t)JSB * BS);
    fsync(fd);
    return "发现已提交事务 → 重放（replay）";
}

// ---------- 检查：类似 fsck 的元数据检查 + 数据内容检查 ----------
static const char *check(char *detail, size_t dn) {
    char bm[BS], in[BS], d5[BS];
    bread(BMAP, bm); bread(INODE, in); bread(DATA0 + 5, d5);
    int size, p[4];
    if (sscanf(in, "INODE size=%d ptr=%d,%d,%d,%d", &size, &p[0], &p[1], &p[2], &p[3]) != 5) {
        snprintf(detail, dn, "inode 块内容=\"%.24s\"", in); return "inode 被垃圾覆盖！";
    }
    int inode_has5 = (p[1] == 5), bmap_has5 = (bm[5] == '1');
    int d5ok = strncmp(d5, "Db:", 3) == 0;
    snprintf(detail, dn, "inode.size=%d ptr[1]=%d, bitmap=%.8s, 块5=\"%.14s\"", size, p[1], bm, d5);
    if (!inode_has5 && !bmap_has5) return d5ok ? "旧状态（一致；Db 写了但没人指向，追加丢失）" : "旧状态（一致；追加丢失）";
    if (inode_has5 && !bmap_has5) return d5ok ? "不一致：inode 用块5，位图说空闲" : "不一致 + inode 指向垃圾";
    if (!inode_has5 && bmap_has5) return "不一致：位图占用块5但无人指向（空间泄漏）";
    return d5ok ? "新状态（一致，追加成功）" : "元数据一致，但 inode 指向垃圾！";
}

static int total_writes(const char *mode) {
    mkfs(); crash_after = -1; nwrites = 0; append(mode); close(fd); return nwrites;
}

// 在子进程里执行 append，写满 n 个块后 _exit，模拟断电（之后的写永远不会发生）
static void run_with_crash(const char *mode, int n) {
    mkfs();
    pid_t pid = fork();
    if (pid == 0) { crash_after = n; append(mode); _exit(0); }
    int st; waitpid(pid, &st, 0);
}

static void crashtest(void) {
    const char *modes[] = { "none", "data", "nobarrier", "ordered" };
    const char *title[] = { "无日志（原地写 I、B、Db）", "数据日志（data journaling）",
                            "数据日志但不等屏障（磁盘把 TxE 排在 Db 前）", "有序元数据日志（ordered）" };
    for (int m = 0; m < 4; m++) {
        int W = total_writes(modes[m]);
        // 拿到这个模式完整的写序列名字
        const char *seq[32]; for (int i = 0; i < W; i++) seq[i] = wname[i];
        printf("\n== %s：共 %d 次块写 ==\n", title[m], W);
        printf("  写序列：");
        for (int i = 0; i < W; i++) printf("%s%s", seq[i], i + 1 < W ? " → " : "\n");
        for (int n = 0; n <= W; n++) {
            run_with_crash(modes[m], n);
            const char *r = recover();
            char det[160]; const char *res = check(det, sizeof det);
            printf("  崩溃于第%2d个写之后 %-10s | 恢复：%s\n", n, n ? seq[n - 1] : "(一个都没写)", r);
            printf("  %25s结果：%s\n", "", res);
            close(fd);
        }
    }
    unlink(IMG);
}

static void dump(void) {
    const char *nm[NBLK] = { "JSB", "J0", "J1", "J2", "J3", "J4", "B", "I", "D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7" };
    char buf[BS];
    for (int b = 0; b < NBLK; b++) { bread(b, buf); printf("  [%2d] %-3s %s\n", b, nm[b], buf); }
}

static void show(const char *mode, int n) {
    run_with_crash(mode, n);
    printf("--- 崩溃后（%s，前 %d 个写已落盘）的磁盘镜像 ---\n", mode, n); dump();
    const char *r = recover();
    char det[160]; const char *res = check(det, sizeof det);
    printf("--- 恢复：%s ---\n", r); dump();
    printf("--- 检查：%s\n    %s\n", res, det);
    close(fd);
}

// ---------- fsync 开销与原子更新 ----------
static double now_us(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e6 + t.tv_nsec / 1e3; }

static void fsyncbench(void) {
    const int N = 200; char buf[4096]; memset(buf, 'x', sizeof buf);
    const char *label[] = { "write()                ", "write() + fdatasync()  ", "write() + fsync()      " };
    for (int k = 0; k < 3; k++) {
        int f = open("bench.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        double t0 = now_us();
        for (int i = 0; i < N; i++) {
            if (write(f, buf, sizeof buf) != sizeof buf) { perror("write"); exit(1); }
            if (k == 1) fdatasync(f);
            if (k == 2) fsync(f);
        }
        double t = now_us() - t0; close(f);
        printf("  %s %4d 次 4KB 追加：平均 %8.1f us/次\n", label[k], N, t / N);
    }
    unlink("bench.dat");
    // 应用层的原子更新：写临时文件 → fsync → rename → fsync 目录
    const int M = 100; double t0 = now_us();
    for (int i = 0; i < M; i++) {
        int f = open("config.tmp", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        char s[64]; int len = snprintf(s, sizeof s, "version=%d\n", i);
        if (write(f, s, len) != len) { perror("write"); exit(1); }
        fsync(f); close(f);                              // 新内容先落盘（"被指向者先写"）
        rename("config.tmp", "config.txt");              // 再原子地切换名字（"指针"）
        int d = open(".", O_RDONLY | O_DIRECTORY); fsync(d); close(d);  // 让目录项的变化也落盘
    }
    printf("  原子更新（tmp+fsync+rename+fsync目录）%d 次：平均 %8.1f us/次\n", M, (now_us() - t0) / M);
    FILE *fp = fopen("config.txt", "r"); char line[64] = "";
    if (fp) { if (!fgets(line, sizeof line, fp)) line[0] = 0; fclose(fp); }
    printf("  config.txt 最终内容：%s", line);
    unlink("config.txt");
}

int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "crashtest")) crashtest();
    else if (argc >= 4 && !strcmp(argv[1], "show")) { show(argv[2], atoi(argv[3])); unlink(IMG); }
    else if (argc >= 2 && !strcmp(argv[1], "fsyncbench")) fsyncbench();
    else {
        fprintf(stderr, "用法: %s crashtest | show {none|data|nobarrier|ordered} N | fsyncbench\n", argv[0]);
        return 2;
    }
    return 0;
}
