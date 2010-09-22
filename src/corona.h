#ifndef __corona_corona_h__
#define __corona_corona_h__

#include <ev.h>
#include "v8-util.h"

/**
 * Base class for all Corona V8 threads.
 */
class CoronaThread : public v8::internal::Thread {
    public:
        CoronaThread(void);

        /**
         * Subclasses should implement Run2() rather than overriding this.
         */
        void Run(void);

        /**
         * Yield until we see some activity on the given fd.
         */
        void YieldIO(int fd, int events);

        /**
         * Mark this thread as runnable (but don't run it).
         */
        void Schedule(void);

    protected:
        /**
         * Storage for different libev watchers.
         *
         * We keep a reference to ourself in this structure so that we can
         * cast back to get it from our callbacks.
         */
        struct ct_ev {
            union {
                struct ev_io ct_io_;
            } ct_u_;

            CoronaThread *ct_self_;
        } ct_ev_;

        /**
         * The libev structure currently in use.
         *
         * Valid values are like such as the EV_PREPARE, EV_IO and everything
         * like such as.
         */
        uint32_t ct_ev_type_;

        /**
         * Subclasses implement this for their logic.
         *
         * This is separated from Run() so that the superclass can implement
         * some V8 and scheduling logic before the threads execute and after
         * they terminate.
         */
        virtual void Run2(void) = 0;

    private:
        void Yield(void);
        static void ReadyCB(struct ev_loop *el, void *evp, int revents);
};

/**
 * Thread for invoking callbacks in their own control flow.
 */
class CallbackThread : public CoronaThread {
    public:
        CallbackThread(v8::Function *cb, uint8_t argc,
                       v8::Handle<v8::Value> argv[]);
        ~CallbackThread(void);
        void Run2(void);

    private:
        v8::Persistent<v8::Function> cb_;
        uint8_t argc_;
        v8::Persistent<v8::Value> *argv_;
};

extern char *g_execname;
extern CoronaThread *g_current_thread;

#endif /* __corona_corona_h__ */
