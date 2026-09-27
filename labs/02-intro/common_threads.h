// common_threads.h —— 带错误检查的 pthread 包装（原书用大写开头的 Pthread_create 等）
#ifndef __common_threads_h__
#define __common_threads_h__

#include <pthread.h>
#include <assert.h>

#define Pthread_create(thread, attr, start_routine, arg) \
    assert(pthread_create(thread, attr, start_routine, arg) == 0)
#define Pthread_join(thread, value_ptr) \
    assert(pthread_join(thread, value_ptr) == 0)

#endif // __common_threads_h__
