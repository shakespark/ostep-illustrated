// common_threads.h —— 仿 OSTEP 的包装函数：调用 pthread 例程并检查返回码，失败就退出。
#ifndef COMMON_THREADS_H
#define COMMON_THREADS_H
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_RC(call) do { int rc_ = (call); \
    if (rc_ != 0) { fprintf(stderr, "%s:%d: %s 失败: %s\n", __FILE__, __LINE__, #call, strerror(rc_)); exit(1); } } while (0)

#define Pthread_create(t, a, f, arg) CHECK_RC(pthread_create(t, a, f, arg))
#define Pthread_join(t, r)           CHECK_RC(pthread_join(t, r))
#define Pthread_mutex_lock(m)        CHECK_RC(pthread_mutex_lock(m))
#define Pthread_mutex_unlock(m)      CHECK_RC(pthread_mutex_unlock(m))
#define Pthread_cond_wait(c, m)      CHECK_RC(pthread_cond_wait(c, m))
#define Pthread_cond_signal(c)       CHECK_RC(pthread_cond_signal(c))

static inline void *Malloc(size_t n) { void *p = malloc(n); if (!p) { perror("malloc"); exit(1); } return p; }
#endif
