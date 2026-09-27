// segv.c —— 真实系统里的"越界异常"：访问没有映射/没有权限的地址时，
// CPU 触发异常 → 内核的异常处理程序接手 → 通常给进程发 SIGSEGV（默认动作：终止进程）。
// 这里我们装一个信号处理函数，把"被 OS 杀掉"变成"打印出错地址后继续"。
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <setjmp.h>
#include <unistd.h>
#include <sys/mman.h>

static sigjmp_buf env;
static void *fault_addr;

static void handler(int sig, siginfo_t *si, void *ctx) {
    (void) sig; (void) ctx;
    fault_addr = si->si_addr;            // 硬件报告的出错虚拟地址
    siglongjmp(env, 1);
}

static void try_read(const char *name, volatile char *p) {
    if (sigsetjmp(env, 1) == 0) {
        char c = *p;                     // 可能触发异常的访问
        printf("%-38s %p  OK (read %d)\n", name, (void *) p, c);
    } else {
        printf("%-38s %p  SIGSEGV! fault address = %p\n", name, (void *) p, fault_addr);
    }
}

int main(void) {
    struct sigaction sa = {0};
    sa.sa_sigaction = handler;
    sa.sa_flags = SA_SIGINFO;
    sigaction(SIGSEGV, &sa, NULL);

    long pg = sysconf(_SC_PAGESIZE);
    // 申请 2 页，把第 2 页设成不可访问：它就像"界限"之外
    char *region = mmap(NULL, 2 * pg, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    mprotect(region + pg, pg, PROT_NONE);
    region[pg - 1] = 7;

    try_read("region[0]          (legal)", region);
    try_read("region[pg-1]       (last legal byte)", region + pg - 1);
    try_read("region[pg]         (1 byte past end)", region + pg);
    try_read("(char*)16          (page 0 unmapped)", (char *) 16);
    try_read("0xffff800000000000 (kernel address)", (char *) 0xffff800000000000UL);
    printf("page size = %ld bytes\n", pg);
    return 0;
}
