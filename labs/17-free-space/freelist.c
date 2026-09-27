// freelist.c —— 按原书 17.2 节，把空闲链表"嵌入"到一块 4KB 的空闲空间里。
// 为了让数字和书上图 17.3~17.7 完全一致：
//   * 头块 header_t = {int size; int magic;}，空闲节点 node_t = {int size; int next;}，都是 8 字节；
//   * next 存的是"书中虚拟地址"（堆起点记作 16384 = 16KB），而不是 64 位指针。
// 分配策略：首次适配（first fit）+ 分割；释放：插到链表头部，不自动合并（最后手动 coalesce）。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#define HEAP_SIZE 4096
#define BOOK_BASE 16384            // 书上假设堆从虚拟地址 16KB 开始
#define MAGIC     1234567

typedef struct { int size; int magic; } header_t;
typedef struct { int size; int next;  } node_t;   // next = 书中地址，0 表示 NULL

static char *heap;                 // mmap 得到的真实内存
static int head;                   // 空闲链表头（书中地址）

static void *at(int addr) { return heap + (addr - BOOK_BASE); }   // 书中地址 → 真实指针
static int addr_of(void *p) { return (int) ((char *) p - heap) + BOOK_BASE; }

static void dump(const char *title) {
    printf("%-34s free list: head", title);
    for (int a = head; a != 0; a = ((node_t *) at(a))->next)
        printf(" -> [addr %d, size %d]", a, ((node_t *) at(a))->size);
    printf(" -> NULL\n");
}

static void *my_malloc(int n) {
    int need = n + (int) sizeof(header_t);          // 要找 N + 头块大小
    int prev = 0;
    for (int a = head; a != 0; prev = a, a = ((node_t *) at(a))->next) {
        node_t *nd = at(a);
        int total = nd->size + (int) sizeof(node_t); // 这个空闲块的总字节数
        if (total < need) continue;                  // 太小，看下一个
        int next = nd->next;
        int rest = total - need;
        int replacement;
        if (rest >= (int) sizeof(node_t)) {          // 分割：剩余部分成为新的空闲节点
            node_t *r = at(a + need);
            r->size = rest - (int) sizeof(node_t);
            r->next = next;
            replacement = a + need;
        } else {                                     // 剩得太少：整块给出去
            need = total;
            replacement = next;
        }
        if (prev == 0) head = replacement; else ((node_t *) at(prev))->next = replacement;
        header_t *h = at(a);
        h->size = need - (int) sizeof(header_t);
        h->magic = MAGIC;
        return (char *) h + sizeof(header_t);        // 返回头块之后的地址
    }
    return NULL;
}

static void my_free(void *ptr) {
    header_t *h = (header_t *) ptr - 1;              // 往回退一个头块
    if (h->magic != MAGIC) { printf("free: bad magic!\n"); return; }
    node_t *nd = (node_t *) h;                       // 这块内存现在变成空闲节点
    int size = h->size;                              // 头块和节点都是 8 字节，size 字段不变
    nd->size = size;
    nd->next = head;                                 // 插到链表头
    head = addr_of(nd);
}

static void coalesce(void) {                         // 按地址排序后合并相邻块
    int addrs[64], n = 0;
    for (int a = head; a != 0; a = ((node_t *) at(a))->next) addrs[n++] = a;
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (addrs[j] < addrs[i]) { int t = addrs[i]; addrs[i] = addrs[j]; addrs[j] = t; }
    head = 0;
    int last = 0;
    for (int i = 0; i < n; i++) {
        node_t *nd = at(addrs[i]);
        if (last && last + ((node_t *) at(last))->size + (int) sizeof(node_t) == addrs[i]) {
            ((node_t *) at(last))->size += nd->size + (int) sizeof(node_t);   // 与前一块相邻：并进去
        } else {
            nd->next = 0;
            if (last) ((node_t *) at(last))->next = addrs[i]; else head = addrs[i];
            last = addrs[i];
        }
    }
}

int main(void) {
    heap = mmap(NULL, HEAP_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    node_t *first = (node_t *) heap;
    first->size = HEAP_SIZE - (int) sizeof(node_t);  // 4096 - 8 = 4088
    first->next = 0;
    head = BOOK_BASE;
    dump("Fig 17.3 init:");

    char *p1 = my_malloc(100);
    printf("ptr1 = malloc(100) = %d\n", addr_of(p1));
    dump("Fig 17.4 after 1 alloc:");
    char *p2 = my_malloc(100);
    char *p3 = my_malloc(100);
    printf("ptr2 = %d, ptr3 = %d\n", addr_of(p2), addr_of(p3));
    dump("Fig 17.5 after 3 allocs:");

    printf("free(%d)\n", addr_of(p2));
    my_free(p2);
    dump("Fig 17.6 after free(ptr2):");
    my_free(p1);
    my_free(p3);
    printf("free(%d), free(%d)\n", addr_of(p1), addr_of(p3));
    dump("Fig 17.7 all freed, no coalesce:");
    char *big = my_malloc(3900);                     // 总共空闲 4064 字节，但没有一块 >= 3908
    printf("malloc(3900) = %s\n", big ? "OK" : "NULL (no single chunk is big enough!)");
    coalesce();
    dump("after coalesce:");
    big = my_malloc(3900);
    if (big) printf("malloc(3900) = %d\n", addr_of(big));
    munmap(heap, HEAP_SIZE);
    return 0;
}
