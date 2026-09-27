// create_args.c —— Figure 27.1/27.2/27.3：给线程传参数、从线程取返回值的三种正确写法
#include "common_threads.h"

typedef struct { int a; int b; } myarg_t;
typedef struct { int x; int y; } myret_t;

// 写法 1+2（Figure 27.1 / 27.2）：参数打包成结构体传入；返回值放在堆上
void *mythread(void *arg) {
    myarg_t *args = (myarg_t *) arg;          // 把 void* 转回真实类型
    printf("  [线程] 收到参数 a=%d b=%d\n", args->a, args->b);
    myret_t *rvals = Malloc(sizeof(myret_t)); // 在堆上分配：线程退出后依然有效
    rvals->x = args->a + 1;
    rvals->y = args->b + 2;
    printf("  [线程] 返回堆地址 %p\n", (void *) rvals);
    return (void *) rvals;
}

// 写法 3（Figure 27.3）：单个整数直接塞进 void*，不需要任何分配
void *mythread_value(void *arg) {
    long long int value = (long long int) arg;
    printf("  [线程] 收到值 %lld\n", value);
    return (void *) (value + 1);
}

int main(void) {
    pthread_t p;
    myret_t *rvals;
    myarg_t args = { 10, 20 };

    printf("== 结构体参数 + 堆上返回值 (Figure 27.2) ==\n");
    Pthread_create(&p, NULL, mythread, &args);  // &args 在 main 的栈上，但 main 会 join，活得比线程久
    Pthread_join(p, (void **) &rvals);           // join 把线程的返回值写进 rvals
    printf("main: returned %d %d (来自地址 %p)\n", rvals->x, rvals->y, (void *) rvals);
    free(rvals);                                 // 谁拿到堆内存谁负责释放

    printf("== 直接传值 (Figure 27.3) ==\n");
    long long int rvalue;
    Pthread_create(&p, NULL, mythread_value, (void *) 100);
    Pthread_join(p, (void **) &rvalue);
    printf("main: returned %lld\n", rvalue);

    printf("== 多个线程，各自的参数 ==\n");
    pthread_t ts[3];
    myarg_t as[3];                               // 每个线程一份独立参数，避免共用一个会被改写的变量
    for (int i = 0; i < 3; i++) { as[i].a = i; as[i].b = i * 100; Pthread_create(&ts[i], NULL, mythread, &as[i]); }
    for (int i = 0; i < 3; i++) {
        Pthread_join(ts[i], (void **) &rvals);
        printf("main: 线程 %d 返回 %d %d\n", i, rvals->x, rvals->y);
        free(rvals);
    }
    return 0;
}
