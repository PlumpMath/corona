#ifndef __corona_corona_h__
#define __corona_corona_h__

#include "v8-util.h"

class CoronaThread : public v8::internal::Thread {
    public:
        CoronaThread();
        virtual void Run();
};

extern char *g_execname;

#endif /* __corona_corona_h__ */
