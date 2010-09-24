#include <list>
#include "corona.h"
#include "sched.h"

CoronaThread *g_current_thread = NULL;

static std::list<CoronaThread*> g_runnableThreads;
static std::list<CoronaThread*> g_zombieThreads;

CoronaThread *
PopRunnableThread(void) {
    CoronaThread *next;

    if (g_runnableThreads.empty()) {
        next = NULL;
    } else {
        next = g_runnableThreads.front();
        g_runnableThreads.pop_front();
    }

    while (!g_zombieThreads.empty()) {
        CoronaThread *zt = g_zombieThreads.front();
        g_zombieThreads.pop_front();

        delete zt;
    }

    return next;
}

void
ScheduleRunnableThread(CoronaThread *ct) {
    g_runnableThreads.push_back(ct);
}

CoronaThread::CoronaThread(void) {
    this->ct_ev_.ct_self_ = this;
    this->ct_ev_type_ = 0;
}

void
CoronaThread::Run(void) {
    ASSERT(v8::internal::current_thread == this);
    ASSERT(g_current_thread == this);

    {
        v8::Locker lock;
        v8::Context::Scope ctx_scope(g_v8Ctx);
        v8::HandleScope scope;

        this->Run2();
    }
    
    // If we get here, it means that the applicatoin code represented by
    // this control flow has returned; we're done. Find someone else to run
    // and clean up after ourselves.

    g_current_thread = PopRunnableThread();
    g_zombieThreads.push_back(this);

    if (g_current_thread) {
        g_current_thread->Start();
    } else {
        ASSERT(g_runnableThreads.empty());
        v8::internal::main_thread->Start();
    }

    UNREACHABLE();
}

void
CoronaThread::Schedule(void) {
    g_runnableThreads.push_back(this);
}

void
CoronaThread::YieldIO(int fd, int events) {
    ASSERT(this->ct_ev_type_ == 0);

    this->ct_ev_type_ = EV_IO;
    ev_io_init(
        &this->ct_ev_.ct_u_.ct_io_,
        (void (*)(struct ev_loop *, ev_io *, int)) CoronaThread::ReadyCB,
        fd,
        events);
    ev_io_start(g_loop, &this->ct_ev_.ct_u_.ct_io_);

    this->Yield();

    this->ct_ev_type_ = 0;
    ev_io_stop(g_loop, &this->ct_ev_.ct_u_.ct_io_);
}

void
CoronaThread::Yield(void) {
    ASSERT(this->ct_ev_type_ != 0);
    ASSERT(g_current_thread == this);
    ASSERT(v8::internal::current_thread == this);

    CoronaThread *next = NULL;

    // Pick another thread to run. If we don't have any, bounce out to the
    // main thread to ask the event loop for more work.
    if ((next = PopRunnableThread())) {

        // If we're runnable, avoid recursing
        if (next == this) {
            return;
        }

        v8::Unlocker unlock;

        g_current_thread = next;
        next->Start();
        ASSERT(g_current_thread == this);
    } else {
        g_current_thread = NULL;
        v8::internal::main_thread->Start();
        ASSERT(g_current_thread == this);
    }
}

void
CoronaThread::ReadyCB(struct ev_loop *el, void *evp, int revents) {
    CoronaThread *self = ((struct ct_ev*) evp)->ct_self_;

    g_runnableThreads.push_back(self);
}

CallbackThread::CallbackThread(v8::Function *cb, uint8_t argc,
                               v8::Handle<v8::Value> argv[]) :
    cb_(cb), argc_(argc) {
    this->argv_ = new v8::Persistent<v8::Value>[argc];

    for (uint8_t i = 0; i < argc; i++) {
        this->argv_[i] = v8::Persistent<v8::Value>::New(argv[i]);
    }
}

CallbackThread::~CallbackThread(void) {
    this->cb_.Dispose();

    for (uint8_t i = 0; i < this->argc_; i++) {
        argv_[i].Dispose();
    }

    delete[] this->argv_;
}

void
CallbackThread::Run2(void) {
    cb_->Call(
        v8::Context::GetCurrent()->Global(),
        this->argc_,
        this->argv_
    );
}
