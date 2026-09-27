// alloc.c —— 覆盖条件（covering condition）：内存分配器例子（原书 Figure 30.15）
//   ./alloc signal    : free() 用 pthread_cond_signal —— 可能叫醒错误的线程
//   ./alloc broadcast : free() 用 pthread_cond_broadcast —— 叫醒所有等待者，各自重新检查
// 场景：堆里 0 字节空闲；Ta 申请 100，Tb 申请 10，都睡下；Tc 释放 50。
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

static int bytesLeft = 0, use_broadcast = 0;
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t c = PTHREAD_COND_INITIALIZER;

static void allocate(const char *who, int size) {
    pthread_mutex_lock(&m);
    struct timespec ts; clock_gettime(CLOCK_REALTIME, &ts); ts.tv_sec += 2;
    while (bytesLeft < size) {
        printf("%s: 需要 %d，只剩 %d → wait\n", who, size, bytesLeft);
        if (pthread_cond_timedwait(&c, &m, &ts) == ETIMEDOUT) {
            printf("%s: 2 秒内没被叫醒（剩余 %d 字节）%s\n", who, bytesLeft,
                   bytesLeft >= size ? " ← 明明够了却没人叫醒我！" : "（本来就不够，正常）");
            pthread_mutex_unlock(&m);
            return;
        }
        printf("%s: 被唤醒，重新检查（剩余 %d）\n", who, bytesLeft);
    }
    bytesLeft -= size;
    printf("%s: 分配 %d 成功，剩余 %d\n", who, size, bytesLeft);
    pthread_mutex_unlock(&m);
}

static void *ta(void *a) { (void)a; allocate("Ta", 100); return NULL; }
static void *tb(void *a) { (void)a; usleep(50000); allocate("Tb", 10); return NULL; }

int main(int argc, char *argv[]) {
    use_broadcast = argc > 1 && strcmp(argv[1], "broadcast") == 0;
    printf("== free() 使用 %s ==\n", use_broadcast ? "pthread_cond_broadcast" : "pthread_cond_signal");
    pthread_t a, b;
    pthread_create(&a, NULL, ta, NULL);
    pthread_create(&b, NULL, tb, NULL);
    usleep(200000);                       // 确保 Ta、Tb 都已睡下（Ta 先入队）
    pthread_mutex_lock(&m);
    bytesLeft += 50;
    printf("Tc: free(50)，剩余 %d → %s\n", bytesLeft, use_broadcast ? "broadcast" : "signal");
    if (use_broadcast) pthread_cond_broadcast(&c); else pthread_cond_signal(&c);
    pthread_mutex_unlock(&m);
    pthread_join(a, NULL);
    pthread_join(b, NULL);
    return 0;
}
