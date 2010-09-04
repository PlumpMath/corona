#ifndef __coro_common_h__
#define __coro_common_h__

#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>

// Debugging

size_t dbg_level = 0;

#define DBG(l, x) \
    do { \
        if ((l) < dbg_level) \
            printf x; \
        } \
    while (0)

// Tracking errno values for syscalls

typedef size_t errno_cnt_t[ELAST + 1];

#define errno_cnt_incr(ecp) \
    do { \
        assert(errno <= ELAST); \
        (*ecp)[errno]++; \
    } while (0)

#define errno_cnt_init(ecp) \
    bzero((ecp), sizeof(errno_cnt_t))

static int errno_cnt_empty(errno_cnt_t *ecp) {
    int i;

    for (i = 0; i <= ELAST && (*ecp)[i] == 0; i++)
        ;

    return (i > ELAST);
}

static void errno_cnt_dump(FILE *fp, char *name, errno_cnt_t *ecp) {
    int i;

    if (errno_cnt_empty(ecp)) {
        return;
    }

    fprintf(fp, "%s: ", name);
    for (i = 0; i <= ELAST; i++) {
        if ((*ecp)[i] == 0) {
            continue;
        }

        fprintf(fp, "%d=%lu ", i, (*ecp)[i]);
    }
    fprintf(fp, "\n");
}

#endif /* __coro_common_h__ */
