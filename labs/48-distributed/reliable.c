// reliable.c —— 在不可靠的 UDP 之上做"可靠通信"：确认（ack）+ 超时/重传 + 序列号去重
//
// 一个程序里 fork 出两个进程：子进程当接收方（receiver），父进程当发送方（sender），
// 都跑在本机 localhost 上。localhost 几乎从不丢包，所以我们在"发送前"按概率 p
// 故意把包丢掉（模拟网络丢包），这样就能观察到超时、重传和重复消息。
//
// 接收方维护一个"账户余额"，每收到一条存款消息就 balance += 100。
// 存款不是幂等操作：同一条消息执行两次，余额就错了——这正是需要序列号的原因。
//
// 用法：./reliable [-p 丢包率] [-t 超时毫秒] [-m 消息条数] [-s 随机种子] [-n]
//   -n  不使用序列号（接收方无法识别重复消息，会重复执行）
#include <arpa/inet.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <unistd.h>
#include "udp.h"

#define SENDER_PORT   20001
#define RECEIVER_PORT 10001

static double loss = 0.3;
static int timeout_ms = 100, nmsgs = 5, use_seq = 1;
static unsigned seed = 1;
static struct timeval t0;

static double now_ms(void) {
    struct timeval t;
    gettimeofday(&t, NULL);
    return (t.tv_sec - t0.tv_sec) * 1000.0 + (t.tv_usec - t0.tv_usec) / 1000.0;
}

// 模拟网络：以概率 loss 丢掉这个包（根本不调用 sendto）
static int lossy_send(int sd, struct sockaddr_in *to, char *buf, unsigned *rs) {
    double r = rand_r(rs) / (RAND_MAX + 1.0);
    if (r < loss) return 0;                 // 丢了：发送方并不会得到任何通知
    UDP_Write(sd, to, buf, (int) strlen(buf) + 1);
    return 1;
}

static void receiver(int sd) {
    unsigned rs = seed * 2 + 1;
    int expected = 1, balance = 0, delivered = 0, dup = 0, acks = 0;
    struct pollfd pfd = { .fd = sd, .events = POLLIN };
    // 1.5 秒内再没有新消息，就认为发送方结束了
    while (poll(&pfd, 1, 1500) > 0) {
        struct sockaddr_in from;
        char buf[BUFFER_SIZE], ack[64];
        if (UDP_Read(sd, &from, buf, sizeof(buf)) <= 0) continue;
        int seq, amount;
        sscanf(buf, "DEPOSIT seq=%d amount=%d", &seq, &amount);
        if (!use_seq || seq == expected) {
            balance += amount;              // 交给"应用"执行（非幂等！）
            delivered++;
            if (use_seq) expected++;
            printf("%8.2f ms  receiver: got seq=%d -> deliver, balance=%d\n", now_ms(), seq, balance);
        } else {
            dup++;                          // seq < expected：以前收过，丢弃但仍然要 ack
            printf("%8.2f ms  receiver: got seq=%d but expected=%d -> DUPLICATE, discard (still ack)\n",
                   now_ms(), seq, expected);
        }
        snprintf(ack, sizeof(ack), "ACK seq=%d", seq);
        acks++;
        if (lossy_send(sd, &from, ack, &rs))
            printf("%8.2f ms  receiver: send ACK seq=%d\n", now_ms(), seq);
        else
            printf("%8.2f ms  receiver: send ACK seq=%d  ... LOST in network\n", now_ms(), seq);
    }
    printf("---- receiver summary: delivered=%d, duplicates discarded=%d, acks sent=%d\n",
           delivered, dup, acks);
    printf("---- final balance = %d (expected %d) %s\n", balance, nmsgs * 100,
           balance == nmsgs * 100 ? "OK" : "<-- WRONG! duplicate executed");
}

static void sender(int sd) {
    unsigned rs = seed * 2;
    struct sockaddr_in to;
    UDP_FillSockAddr(&to, "localhost", RECEIVER_PORT);
    int total = 0;
    for (int i = 1; i <= nmsgs; i++) {
        char msg[64];
        int seq = use_seq ? i : 0;          // 没有序列号时，所有消息看起来都一样
        snprintf(msg, sizeof(msg), "DEPOSIT seq=%d amount=100", seq);
        for (int attempt = 1;; attempt++) {
            total++;
            // 保留一份消息副本（msg 就是），发出去，并设置定时器
            if (lossy_send(sd, &to, msg, &rs))
                printf("%8.2f ms  sender:   send msg #%d (seq=%d, try %d)\n", now_ms(), i, seq, attempt);
            else
                printf("%8.2f ms  sender:   send msg #%d (seq=%d, try %d)  ... LOST in network\n",
                       now_ms(), i, seq, attempt);
            double deadline = now_ms() + timeout_ms;
            int acked = 0;
            while (!acked) {
                int left = (int) (deadline - now_ms());
                struct pollfd pfd = { .fd = sd, .events = POLLIN };
                // 用 poll 睡眠等待 ack 或超时，不空转浪费 CPU
                if (left <= 0 || poll(&pfd, 1, left) == 0) break;
                char buf[64];
                struct sockaddr_in from;
                if (UDP_Read(sd, &from, buf, sizeof(buf)) <= 0) continue;
                int aseq;
                sscanf(buf, "ACK seq=%d", &aseq);
                if (aseq == seq) {
                    acked = 1;
                    printf("%8.2f ms  sender:   got ACK seq=%d -> msg #%d done, delete copy, timer off\n",
                           now_ms(), aseq, i);
                } else {
                    printf("%8.2f ms  sender:   got stale ACK seq=%d, ignore\n", now_ms(), aseq);
                }
            }
            if (acked) break;
            printf("%8.2f ms  sender:   TIMEOUT for msg #%d -> retry\n", now_ms(), i);
        }
    }
    printf("---- sender summary: %d messages, %d transmissions (%d retries), seq numbers %s\n",
           nmsgs, total, total - nmsgs, use_seq ? "ON" : "OFF");
}

int main(int argc, char *argv[]) {
    int c;
    while ((c = getopt(argc, argv, "p:t:m:s:n")) != -1) {
        switch (c) {
        case 'p': loss = atof(optarg); break;
        case 't': timeout_ms = atoi(optarg); break;
        case 'm': nmsgs = atoi(optarg); break;
        case 's': seed = (unsigned) atoi(optarg); break;
        case 'n': use_seq = 0; break;
        default:
            fprintf(stderr, "usage: %s [-p loss] [-t timeout_ms] [-m msgs] [-s seed] [-n]\n", argv[0]);
            return 1;
        }
    }
    setvbuf(stdout, NULL, _IOLBF, 0);       // 行缓冲：两个进程的输出按行交错
    printf("loss=%.2f timeout=%dms msgs=%d seed=%u seq=%s\n", loss, timeout_ms, nmsgs, seed,
           use_seq ? "on" : "off");
    // 先在父进程里绑定好两个端口，再 fork，保证接收方一定已就绪
    int rsd = UDP_Open(RECEIVER_PORT), ssd = UDP_Open(SENDER_PORT);
    if (rsd < 0 || ssd < 0) { perror("UDP_Open"); return 1; }
    gettimeofday(&t0, NULL);
    pid_t pid = fork();
    if (pid == 0) {                         // 子进程：接收方
        UDP_Close(ssd);
        receiver(rsd);
        return 0;
    }
    UDP_Close(rsd);                          // 父进程：发送方
    sender(ssd);
    waitpid(pid, NULL, 0);
    return 0;
}
