// sched.c —— 仿照 OSTEP 作业 scheduler.py 的单 CPU 调度模拟器（C 版）
// 用法: ./sched -p FIFO|SJF|RR [-q 时间片] -l 长度1,长度2,... [-s]
// 假设与原书 7.1 节一致：所有作业在 0 时刻到达（按列表顺序"一根头发丝"地先后到达），只用 CPU。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAXJ 64

typedef struct { int id; double len, rem, first, done; } job_t;

static void usage(const char *p) {
    fprintf(stderr, "usage: %s -p FIFO|SJF|RR [-q quantum] -l len1,len2,... [-s]\n", p);
    exit(1);
}

int main(int argc, char *argv[]) {
    const char *policy = "FIFO";
    double quantum = 1.0;
    char *list = NULL;
    int stats_only = 0, c;
    while ((c = getopt(argc, argv, "p:q:l:s")) != -1) {
        switch (c) {
        case 'p': policy = optarg; break;
        case 'q': quantum = atof(optarg); break;
        case 'l': list = optarg; break;
        case 's': stats_only = 1; break;
        default: usage(argv[0]);
        }
    }
    if (!list || quantum <= 0) usage(argv[0]);

    job_t jobs[MAXJ];
    int n = 0;
    for (char *tok = strtok(list, ","); tok && n < MAXJ; tok = strtok(NULL, ",")) {
        jobs[n] = (job_t){ .id = n, .len = atof(tok), .rem = atof(tok), .first = -1, .done = -1 };
        n++;
    }

    int order[MAXJ];                      // 运行顺序（FIFO/SJF）或就绪队列（RR）
    for (int i = 0; i < n; i++) order[i] = i;
    if (strcmp(policy, "SJF") == 0) {     // 稳定排序：长度相同保持到达顺序
        for (int i = 1; i < n; i++)
            for (int j = i; j > 0 && jobs[order[j]].len < jobs[order[j - 1]].len; j--) {
                int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
            }
    } else if (strcmp(policy, "FIFO") != 0 && strcmp(policy, "RR") != 0) usage(argv[0]);

    if (strcmp(policy, "RR") == 0) printf("** Policy: RR   quantum: %g   jobs: %d\n", quantum, n);
    else printf("** Policy: %s   jobs: %d\n", policy, n);

    double t = 0;
    if (strcmp(policy, "RR") != 0) {
        for (int k = 0; k < n; k++) {
            job_t *j = &jobs[order[k]];
            j->first = t;
            if (!stats_only)
                printf("  [ time %3.0f ] Run job %d for %.2f secs ( DONE at %.2f )\n", t, j->id, j->len, t + j->len);
            t += j->len;
            j->done = t;
        }
    } else {
        int q[MAXJ * 2], head = 0, cnt = n;  // 循环队列
        for (int i = 0; i < n; i++) q[i] = i;
        while (cnt > 0) {
            job_t *j = &jobs[q[head]];
            head = (head + 1) % (MAXJ * 2);
            cnt--;
            if (j->first < 0) j->first = t;
            double run = j->rem < quantum ? j->rem : quantum;
            j->rem -= run;
            t += run;
            if (!stats_only)
                printf("  [ time %3.0f ] Run job %3d for %.2f secs%s\n", t - run, j->id, run,
                       j->rem <= 0 ? " ( DONE )" : "");
            if (j->rem <= 0) j->done = t;
            else { q[(head + cnt) % (MAXJ * 2)] = j->id; cnt++; }
        }
    }

    double sr = 0, st = 0, sw = 0;
    printf("\nFinal statistics:\n");
    for (int i = 0; i < n; i++) {
        double resp = jobs[i].first, turn = jobs[i].done, wait = turn - jobs[i].len;
        printf("  Job %3d -- Response: %.2f  Turnaround %.2f  Wait %.2f\n", i, resp, turn, wait);
        sr += resp; st += turn; sw += wait;
    }
    printf("\n  Average -- Response: %.2f  Turnaround %.2f  Wait %.2f\n\n", sr / n, st / n, sw / n);
    return 0;
}
