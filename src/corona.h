#ifndef __corona_corona_h__
#define __corona_corona_h__

#include <ev.h>
#include "v8-util.h"

class CoronaThread : public v8::internal::Thread {
    public:
        CoronaThread(void);
        virtual void Run(void);

        void YieldIO(int fd, int events);

    protected:
        // Our libev structure
        struct ct_ev {
            union {
                struct ev_io ct_io_;
                struct ev_prepare ct_prepare_;
            } ct_u_;

            CoronaThread *ct_self_;
        } ct_ev_;

        // The libev structure currently in use. Valid values are like such
        // as the EV_PREPARE, EV_IO and everything like such as.
        uint32_t ct_ev_type_;

    private:
        void Yield(void);
        static void ReadyCB(struct ev_loop *el, void *evp, int revents);
};

extern char *g_execname;
extern CoronaThread *g_currentThread;

#endif /* __corona_corona_h__ */
