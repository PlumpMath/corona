#ifndef __coro_common_h__
#define __coro_common_h__

#include <stdio.h>
#include <sys/types.h>

size_t dbg_level = 0;

#define DBG(l, x) \
    do { \
        if ((l) < dbg_level) \
            printf x; \
        } \
    while (0)

#endif /* __coro_common_h__ */
