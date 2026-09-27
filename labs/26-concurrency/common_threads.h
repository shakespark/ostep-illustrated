// 仿 OSTEP ostep-code 的 common_threads.h：包装 pthread 调用，出错就退出。
#ifndef __COMMON_THREADS_H__
#define __COMMON_THREADS_H__

#include <pthread.h>
#include <assert.h>

#define Pthread_create(thread, attr, start_routine, arg) \
    assert(pthread_create(thread, attr, start_routine, arg) == 0);
#define Pthread_join(thread, value_ptr) \
    assert(pthread_join(thread, value_ptr) == 0);
#define Pthread_mutex_lock(m)   assert(pthread_mutex_lock(m) == 0);
#define Pthread_mutex_unlock(m) assert(pthread_mutex_unlock(m) == 0);

#endif
