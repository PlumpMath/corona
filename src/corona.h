#ifndef __corona_corona_h__
#define __corona_corona_h__

#include <v8.h>

struct ev_loop;

extern char *g_execname;
extern struct ev_loop *g_loop;

extern v8::Persistent<v8::Context> g_v8Ctx;

#endif /* __corona_corona_h__ */
