// bad_return.c —— 错误示范：返回指向栈上局部变量的指针（原书 "oops" 例子）
//   ./bad_return            线程返回 &oops，main 在 join 之后读它
//   ./bad_return helper     线程内部调用一个返回 &局部变量 的函数，再读它
#include "common_threads.h"

typedef struct { int x; int y; } myret_t;

void *mythread(void *arg) {
    (void) arg;
    myret_t oops;            // 分配在线程栈上：BAD!
    oops.x = 1;
    oops.y = 2;
    myret_t *p = &oops;      // 借一个中间变量，想绕过 gcc 的警告（gcc 14 依然会警告 dangling pointer）
    return (void *) p;       // 线程返回后，这个栈帧（乃至整个线程栈）就被回收了
}

// 同样的错误，发生在普通函数调用里
__attribute__((noinline)) myret_t *make_ret(void) {
    myret_t local = { 1, 2 };
    myret_t *p = &local;
    return p;                // 函数返回后，local 所在的栈帧失效
}

// 一个"无辜"的函数：在栈上写一堆数据，模拟之后的其它调用复用那块栈空间
int scribble(void) {
    volatile int junk[64];
    int s = 0;
    for (int i = 0; i < 64; i++) { junk[i] = 0xdead0000 + i; s += junk[i]; }
    return s;
}

void *helper_thread(void *arg) {
    (void) arg;
    myret_t *r = make_ret();
    scribble();
    printf("helper: 读到 %d %d （期望 1 2）\n", r->x, r->y);
    return NULL;
}

int main(int argc, char *argv[]) {
    pthread_t p;
    if (argc > 1 && strcmp(argv[1], "helper") == 0) {
        Pthread_create(&p, NULL, helper_thread, NULL);
        Pthread_join(p, NULL);
        return 0;
    }
    myret_t *rvals;
    Pthread_create(&p, NULL, mythread, NULL);
    Pthread_join(p, (void **) &rvals);
    scribble();
    printf("main: rvals 指向 %p\n", (void *) rvals);
    printf("main: returned %d %d （期望 1 2）\n", rvals->x, rvals->y);  // 悬空指针读
    return 0;
}
