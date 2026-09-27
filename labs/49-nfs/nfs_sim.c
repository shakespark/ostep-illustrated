// nfs_sim.c —— 用一个小型 C 模拟器体会 NFSv2 的四个设计要点（单进程、确定性输出）
//   1. 无状态协议：服务器在两次 READ 之间崩溃重启，客户端照常读完；对比"有状态"协议会失败
//   2. 幂等性：回复丢失后客户端重试。WRITE（带偏移）重试安全；APPEND、MKDIR 重试出问题
//   3. 文件句柄里的世代号（generation）：inode 被重用后，旧句柄得到 ESTALE 而不是读到别人的文件
//   4. 服务器写缓冲：先回复再落盘 + 崩溃 = 客户端以为写成功的数据丢了
// 用法：./nfs_sim [1|2|3|4]   不带参数则运行全部
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ------------------------------ 服务器端 ------------------------------
#define NINODE 16
typedef struct { int vol, ino, gen; } fh_t;            // 文件句柄：卷 ID + inode 号 + 世代号
typedef struct { int used, gen, isdir, size; char data[64]; } inode_t;
typedef struct { char name[16]; int ino; } dent_t;

static inode_t itab[NINODE];                          // 服务器磁盘上的 inode
static dent_t root[8];                                // 根目录（inode 2）的目录项
static int check_gen = 1;                             // 服务器是否检查世代号

static int alloc_inode(int isdir) {
    for (int i = 3; i < NINODE; i++)
        if (!itab[i].used) {
            itab[i].used = 1; itab[i].gen++; itab[i].isdir = isdir; itab[i].size = 0;
            memset(itab[i].data, 0, sizeof(itab[i].data));
            return i;
        }
    return -1;
}
static int dir_find(const char *name) {
    for (int i = 0; i < 8; i++) if (root[i].ino && strcmp(root[i].name, name) == 0) return i;
    return -1;
}
static void dir_add(const char *name, int ino) {
    for (int i = 0; i < 8; i++) if (!root[i].ino) { snprintf(root[i].name, 16, "%s", name); root[i].ino = ino; return; }
}
static void fs_init(void) {
    memset(itab, 0, sizeof(itab)); memset(root, 0, sizeof(root));
    itab[2].used = 1; itab[2].gen = 1; itab[2].isdir = 1;
    int ino = alloc_inode(0);                         // foo：40 字节
    dir_add("foo", ino);
    itab[ino].size = 40;
    memcpy(itab[ino].data, "0123456789ABCDEFGHIJ0123456789abcdefghij", 40);
}
static fh_t root_fh(void) { fh_t f = { 1, 2, itab[2].gen }; return f; }
static int fh_ok(fh_t f) {                             // 用句柄定位 inode（无需任何"打开文件"状态）
    if (f.ino <= 0 || f.ino >= NINODE || !itab[f.ino].used) return 0;
    if (check_gen && itab[f.ino].gen != f.gen) return 0;   // 世代号不符：旧句柄
    return 1;
}
static void pfh(fh_t f) { printf("FH{vol=%d,ino=%d,gen=%d}", f.vol, f.ino, f.gen); }

// NFSPROC_LOOKUP(dir FH, name) -> FH
static int s_lookup(fh_t dir, const char *name, fh_t *out) {
    (void) dir;
    int k = dir_find(name);
    if (k < 0) return -1;
    out->vol = 1; out->ino = root[k].ino; out->gen = itab[root[k].ino].gen;
    return 0;
}
// NFSPROC_READ(FH, offset, count) -> data
static int s_read(fh_t f, int off, int cnt, char *buf) {
    if (!fh_ok(f)) return -2;                          // ESTALE
    inode_t *ip = &itab[f.ino];
    if (off >= ip->size) return 0;
    if (off + cnt > ip->size) cnt = ip->size - off;
    memcpy(buf, ip->data + off, cnt);
    return cnt;
}
// NFSPROC_WRITE(FH, offset, count, data)：数据写到哪里由请求中的 offset 决定 —— 幂等
static int s_write(fh_t f, int off, const char *d, int cnt) {
    if (!fh_ok(f)) return -2;
    inode_t *ip = &itab[f.ino];
    memcpy(ip->data + off, d, cnt);
    if (off + cnt > ip->size) ip->size = off + cnt;
    return cnt;
}
// 假想的 APPEND(FH, data)：写到"当前文件末尾" —— 不幂等（NFSv2 里没有这个操作）
static int s_append(fh_t f, const char *d, int cnt) {
    if (!fh_ok(f)) return -2;
    return s_write(f, itab[f.ino].size, d, cnt);
}
// NFSPROC_MKDIR(dir FH, name)：目录已存在就报错 —— 很难做成幂等
static int s_mkdir(fh_t dir, const char *name) {
    (void) dir;
    if (dir_find(name) >= 0) return -17;              // EEXIST
    dir_add(name, alloc_inode(1));
    return 0;
}
static int s_remove(fh_t dir, const char *name) {
    (void) dir;
    int k = dir_find(name);
    if (k < 0) return -2;
    itab[root[k].ino].used = 0;
    root[k].ino = 0;
    return 0;
}
static void show_file(const char *label, fh_t f) {
    printf("    %s: size=%d data=\"%.*s\"\n", label, itab[f.ino].size, itab[f.ino].size, itab[f.ino].data);
}

// ------------------------------ 1. 无状态 vs 有状态 ------------------------------
static void part1(void) {
    printf("=== 1. Stateless READ survives a server crash (Figure 49.5) ===\n");
    fs_init();
    // 客户端的打开文件表：fd -> (文件句柄, 当前位置)。状态全在客户端。
    struct { int used; fh_t fh; int pos; } oft[4] = { { 0 } };
    fh_t fh;
    printf("app: fd = open(\"/foo\")\n");
    printf("  client -> LOOKUP(rootdir "); pfh(root_fh()); printf(", \"foo\")\n");
    s_lookup(root_fh(), "foo", &fh);
    printf("  server <- returns "); pfh(fh); printf(" + attributes (size=%d)\n", itab[fh.ino].size);
    int fd = 3; oft[fd].used = 1; oft[fd].fh = fh; oft[fd].pos = 0;
    printf("  client: open file table[fd=3] = { fh, pos=0 }\n");
    const int MAX = 16;
    for (int r = 1; r <= 3; r++) {
        char buf[64];
        printf("app: read(fd=3, buf, %d)\n", MAX);
        printf("  client -> READ("); pfh(oft[fd].fh); printf(", offset=%d, count=%d)\n", oft[fd].pos, MAX);
        int n = s_read(oft[fd].fh, oft[fd].pos, MAX, buf);
        printf("  server <- %d bytes \"%.*s\"\n", n, n, buf);
        oft[fd].pos += n;
        printf("  client: pos = %d\n", oft[fd].pos);
        if (r == 1) printf("  *** server crashes and reboots here (all server memory lost) ***\n");
    }
    printf("app: close(fd=3)  -> client frees fd locally, no message to server\n");

    printf("\n--- same program over a hypothetical STATEFUL protocol ---\n");
    int sfd_table_used = 0, sfd_pos = 0;               // 服务器内存中的"打开文件表"
    printf("  client -> OPEN(\"/foo\")\n");
    sfd_table_used = 1; sfd_pos = 0;
    printf("  server <- sfd=7 (server remembers: sfd 7 = foo, pos 0)\n");
    for (int r = 1; r <= 2; r++) {
        printf("  client -> READ(sfd=7, count=%d)\n", MAX);
        if (!sfd_table_used) { printf("  server <- ERROR: unknown descriptor 7 (state lost in crash!)\n"); break; }
        printf("  server <- %d bytes, server pos now %d\n", MAX, sfd_pos += MAX);
        if (r == 1) { printf("  *** server crashes and reboots here ***\n"); sfd_table_used = 0; }
    }
    printf("\n");
}

// ------------------------------ 2. 幂等性与重试 ------------------------------
// 模拟一次"回复丢失"的 RPC：服务器执行了请求，但回复在网络中丢失，客户端超时后重发同一请求
enum { OP_WRITE, OP_APPEND, OP_MKDIR, OP_REMOVE };
static int exec_op(int op, fh_t f) {
    switch (op) {
    case OP_WRITE:  return s_write(f, 2, "abc", 3);
    case OP_APPEND: return s_append(f, "abc", 3);
    case OP_MKDIR:  return s_mkdir(root_fh(), "newdir");
    default:        return s_remove(root_fh(), "foo");
    }
}
static void part2(void) {
    const char *names[] = { "WRITE(fh, offset=2, count=3, \"abc\")", "APPEND(fh, \"abc\")  [hypothetical]",
                            "MKDIR(rootdir, \"newdir\")", "REMOVE(rootdir, \"foo\")" };
    printf("=== 2. Retrying after a LOST REPLY (Figure 49.6, case 3) ===\n");
    for (int op = 0; op < 4; op++) {
        fs_init();
        fh_t f = { 0, 0, 0 }; s_lookup(root_fh(), "foo", &f);
        itab[f.ino].size = 8; memcpy(itab[f.ino].data, "xxxxxxxx", 8);
        printf("%s\n", names[op]);
        if (op <= OP_APPEND) show_file("before", f);
        int r1 = exec_op(op, f);
        printf("    try 1: server executes, result=%d, reply LOST -> client times out, retries\n", r1);
        int r2 = exec_op(op, f);
        printf("    try 2: server executes again, result=%d%s -> client sees this result\n", r2,
               r2 == -17 ? " (EEXIST)" : r2 == -2 ? " (ENOENT)" : "");
        if (op <= OP_APPEND) show_file("after ", f);
        const char *verdict = op == OP_WRITE ? "idempotent: same as executing once"
                            : op == OP_APPEND ? "NOT idempotent: data appended twice"
                            : "NOT idempotent: op succeeded, but client is told it failed";
        printf("    => %s\n", verdict);
    }
    printf("\n");
}

// ------------------------------ 3. 世代号 ------------------------------
static void part3(void) {
    printf("=== 3. Generation number in the file handle ===\n");
    for (check_gen = 1; check_gen >= 0; check_gen--) {
        fs_init();
        fh_t old = { 0, 0, 0 };
        s_lookup(root_fh(), "foo", &old);
        printf("[server %s generation]\n", check_gen ? "CHECKS" : "IGNORES");
        printf("  client A caches handle for foo: "); pfh(old); printf("\n");
        s_remove(root_fh(), "foo");
        int ino = alloc_inode(0);
        dir_add("secret", ino);
        fh_t nf = { 1, ino, itab[ino].gen };
        s_write(nf, 0, "TOP-SECRET", 10);
        printf("  client B: rm foo; create secret -> reuses inode %d, new handle ", ino); pfh(nf); printf("\n");
        char buf[64];
        int n = s_read(old, 0, 16, buf);
        if (n == -2) printf("  client A: READ(old handle) -> ESTALE (stale file handle), good\n");
        else printf("  client A: READ(old handle) -> \"%.*s\"  <-- read someone else's file!\n", n, buf);
    }
    check_gen = 1;
    printf("\n");
}

// ------------------------------ 4. 服务器写缓冲 ------------------------------
static void part4(void) {
    printf("=== 4. Server-side write buffering + crash ===\n");
    for (int safe = 0; safe <= 1; safe++) {
        char disk[3][9] = { "xxxxxxxx", "yyyyyyyy", "zzzzzzzz" };
        char mem[3][9] = { "", "", "" };
        const char *w[3] = { "aaaaaaaa", "bbbbbbbb", "cccccccc" };
        printf("[%s]\n", safe ? "commit to disk BEFORE replying (correct)" : "reply BEFORE commit (wrong)");
        for (int i = 0; i < 3; i++) {
            printf("  WRITE block %d = %s: ", i, w[i]);
            if (safe) {
                if (i == 1) {
                    printf("server crashes before commit -> no reply; client times out, retries; ");
                }
                strcpy(disk[i], w[i]);
                printf("server forces to disk, replies OK\n");
            } else {
                strcpy(mem[i], w[i]);
                printf("server buffers in memory, replies OK");
                if (i == 0) { strcpy(disk[0], mem[0]); mem[0][0] = 0; printf("; later flushed to disk"); }
                if (i == 1) { printf("\n  *** server crashes: memory lost (block 1 never reached disk) ***"); mem[1][0] = 0; }
                if (i == 2) { strcpy(disk[2], mem[2]); mem[2][0] = 0; printf("; later flushed to disk"); }
                printf("\n");
            }
        }
        printf("  client believes all 3 writes succeeded. Final file on server disk:\n");
        for (int i = 0; i < 3; i++)
            printf("    %s%s\n", disk[i], strcmp(disk[i], w[i]) ? "   <--- oops" : "");
    }
}

int main(int argc, char *argv[]) {
    int which = argc > 1 ? atoi(argv[1]) : 0;
    if (!which || which == 1) part1();
    if (!which || which == 2) part2();
    if (!which || which == 3) part3();
    if (!which || which == 4) part4();
    return 0;
}
