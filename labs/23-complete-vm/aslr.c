// aslr.c —— 观察地址空间布局随机化（ASLR）
// 打印栈、堆、mmap 区、代码（PIE）、共享库中的地址。多运行几次对比；
// 用 `setarch -R ./aslr`（或 `setarch $(uname -m) -R ./aslr`）关闭 ASLR 再运行，地址将固定不变。
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>

int global = 42;

int main(void) {
    int stack = 0;
    void *heap = malloc(16);
    void *map = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    printf("stack  %p   heap  %p   mmap  %p   code(main)  %p   data  %p   libc(printf)  %p\n",
           (void *)&stack, heap, map, (void *)main, (void *)&global, (void *)printf);
    return 0;
}
