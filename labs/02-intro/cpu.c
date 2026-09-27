// cpu.c —— 原书 Figure 2.1：每隔约 1 秒打印一次命令行参数，永远循环（Ctrl-C 结束）
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <assert.h>
#include "common.h"

int
main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: cpu <string>\n");
        exit(1);
    }
    char *str = argv[1];
    setvbuf(stdout, NULL, _IOLBF, 0); // 输出重定向到管道/文件时也按行刷新
    while (1) {
        Spin(1);
        printf("%s\n", str);
    }
    return 0;
}
