// common.h —— 与原书 ostep-code/intro/common.h 等价：GetTime() 与 Spin()
#ifndef __common_h__
#define __common_h__

#include <sys/time.h>
#include <sys/stat.h>
#include <assert.h>

static inline double GetTime(void) {
    struct timeval t;
    int rc = gettimeofday(&t, NULL);
    assert(rc == 0);
    return (double) t.tv_sec + (double) t.tv_usec / 1e6;
}

// 忙等 howlong 秒：反复读时钟，直到过去了 howlong 秒（一直占着 CPU）
static inline void Spin(int howlong) {
    double t = GetTime();
    while ((GetTime() - t) < (double) howlong)
        ; // do nothing in loop
}

#endif // __common_h__
