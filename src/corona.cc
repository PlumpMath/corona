#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/param.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <ctype.h>
#include <errno.h>
#include <string>
#include <list>
#include <ev.h>
#include "corona.h"
#include "v8-util.h"

char *g_execname = NULL;
CoronaThread *g_current_thread = NULL;

static struct ev_loop *g_loop = NULL;
static v8::Persistent<v8::Context> g_v8Ctx;
static v8::Persistent<v8::Object> g_sysObj;

static std::list<CoronaThread*> g_runnableThreads;

extern void InitSyscalls(v8::Handle<v8::Object> target);
static v8::Local<v8::Value> ExecFile(const char *);

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

    if (g_runnableThreads.empty()) {
        g_current_thread = NULL;
        v8::internal::main_thread->Start();
        UNREACHABLE();
    }

    // TODO: Delete this thread object; might have to be careful with the
    //       stack.
    //       Maybe queue these for deletion in the prepare loop?
    // TODO: Run the next thread from the run queue

    UNIMPLEMENTED();
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

    // If there are no more threads to run, run through our event loop
    // to collect some. We are guaranteed to have had *some* success here
    // because we block if none are available, and we know(!) that at least
    // one thread exists, us.
    if (g_runnableThreads.empty()) {
        ev_loop(g_loop, EVLOOP_ONESHOT);
        ASSERT(!g_runnableThreads.empty());
    }

    CoronaThread *next = g_runnableThreads.front();
    g_runnableThreads.pop_front();
    if (next != this) {
        v8::Unlocker unlock;

        g_current_thread = next;
        next->Start();
        ASSERT(g_current_thread == this);
        ASSERT(v8::internal::current_thread == this);
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
        g_v8Ctx->Global(),
        this->argc_,
        this->argv_
    );
}

/**
 * Thread for running the script provided on the commandline.
 *
 * This thread is special, as it has to negotiate the boot-up sequence in
 * the main() function and its strange interactions with the event loop.
 */
class AppThread : public CoronaThread {
    public:
        AppThread(const std::string &str);

    protected:
        void Run2(void);

    private:
        const std::string path_;
};

AppThread::AppThread(const std::string &str) : path_(str) {
}

void
AppThread::Run2(void) {
    v8::Handle<v8::Value> val = ExecFile(path_.c_str());
    ASSERT(!val.IsEmpty());
}

static void
DispatchCB(struct ev_loop *el, struct ev_prepare *ep, int revents) {
    if (g_runnableThreads.empty()) {
        return;
    }

    g_current_thread = g_runnableThreads.front();
    g_runnableThreads.pop_front();

    g_current_thread->Start();
}

// atexit() handler; tears down all global state
static void
ExitCB(void) {
    if (!g_v8Ctx.IsEmpty()) {
        g_v8Ctx.Dispose();
        v8::V8::Dispose();
    }
}

// v8::V8::SetFatalErrorHandler() handler
static void
FatalErrorCB(const char *loc, const char *msg) {
    fprintf(stderr, "%s: %s %s\n", g_execname, ((loc) ? loc : ""), msg);
    exit(1);
}

// Log an exception to stderr
static void
LogException(FILE *fp, v8::TryCatch &try_catch) {
    v8::HandleScope scope;

    assert(try_catch.HasCaught());

    v8::Local<v8::Message> msg = try_catch.Message();
    if (!msg.IsEmpty()) {
        v8::String::Utf8Value msgStr(msg->Get());
        v8::String::Utf8Value resourceStr(msg->GetScriptResourceName());

        fprintf(
            fp,
            "%s: %s at %s:%d,%d\n",
                g_execname, *msgStr, *resourceStr, msg->GetLineNumber(),
                msg->GetStartColumn()
        );
    } else {
        v8::Local<v8::Value> ex = try_catch.Exception();
        assert(!ex.IsEmpty());

        fprintf(
            fp,
            "%s: %s\n",
                g_execname, *v8::String::AsciiValue(ex)
        );
    }
}

// Load the contents of the given file into a v8::String
//
// Returns an empty handle on error.
static v8::Local<v8::String>
ReadFile(const char *fname) {
    v8::Local<v8::String> str;
    int fd = -1;
    char *buf = NULL;
    size_t buf_sz = 4;
    size_t buf_off = 0;
    ssize_t nread = 0;

    // Open our script file
    fd = (strcmp(fname, "-")) ?
        open(fname, O_RDONLY) :
        0;
    if (fd < 0) {
        fprintf(
            stderr,
            "%s: ReadFile open failed: %s\n",
                g_execname, strerror(errno)
        );
        return str;
    }

    // Read our script file, resizing the buffer as necessary
    buf = (char*) malloc(buf_sz);
    while ((nread = read(fd, buf + buf_off, buf_sz - buf_off)) > 0) {
        assert(buf_off + nread <= buf_sz);

        buf_off += nread;
    
        if (buf_off == buf_sz) {
            buf_sz <<= 1;
            char *buf2 = (char*) malloc(buf_sz);
            memcpy(buf2, buf, buf_sz);
            free(buf);
            buf = buf2;
        }
    }

    if (nread >= 0) {
        str = v8::String::New(buf, buf_off);
    } else {
        fprintf(
            stderr,
            "%s: ReadFile read failed: %s\n",
                g_execname, strerror(errno)
        );
    }

    free(buf);
    if (fd > 0) {
        close(fd);
    }

    return str;
}

// Execute the JavaScript file of the given name
//
// The script is run in the current V8 context and its result value is
// returned. On error, an empty value is returned.
static v8::Local<v8::Value>
ExecFile(const char *fname) {
    v8::HandleScope scope;
    v8::Local<v8::Value> scriptResult;
    v8::TryCatch try_catch;

    try_catch.SetVerbose(true);
    try_catch.SetCaptureMessage(true);

    v8::Local<v8::String> fileContents = ReadFile(fname);
    if (fileContents.IsEmpty()) {
        return scriptResult;
    }

    v8::Local<v8::Script> script =
        v8::Script::Compile(fileContents, v8::String::New(fname));
    if (script.IsEmpty()) {
        assert(try_catch.HasCaught());
        LogException(stderr, try_catch);

        return scriptResult;
    }

    scriptResult = script->Run();
    if (scriptResult.IsEmpty()) {
        assert(try_catch.HasCaught());
        LogException(stderr, try_catch);

        return scriptResult;
    }

    return scriptResult;
}

// Get the path to our JavaScript libraries
//
// XXX: This always return $PWD/lib. Fix.
static char *
GetLibPath(char *buf, size_t buf_len) {
    char *cwd = getcwd(NULL, MAXPATHLEN);

    if (snprintf(buf, buf_len, "%s/%s", cwd, "lib") >= (int) buf_len) {
        buf = NULL;
    }

    free(cwd);
    return buf;
}

// Get the path to our boot.js file
static char *
GetBootLibPath(char *buf, size_t buf_len) {
    char lib_buf[MAXPATHLEN];

    if (!GetLibPath(lib_buf, sizeof(lib_buf))) {
        return NULL;
    }

    if (snprintf(buf, buf_len, "%s/%s", lib_buf, "boot.js") >= (int) buf_len) {
        return NULL;
    }

    return buf;
}


// TODO: Parse arguments using FlagList::SetFlagsFromCommandLine(); use '--' to
//       delimit V8 options from corona options
int
main(int argc, char *argv[]) {
    char boot_path[MAXPATHLEN];
    struct ev_prepare dispatch;
    AppThread app_thread(argv[1]);

    // Our cleanup handler is smart enough to avoid attempting to clean up
    // things that have not yet been initialized
    atexit(ExitCB);

    g_execname = basename(argv[0]);

    // Initialize V8
    {
        v8::Locker lock;

        v8::V8::Initialize();
        v8::V8::SetFatalErrorHandler(FatalErrorCB);

        g_v8Ctx = v8::Context::New();

        v8::Context::Scope ctx_scope(g_v8Ctx);
        v8::HandleScope scope;

        g_sysObj = v8::Persistent<v8::Object>::New(
            CreateNamespace(g_v8Ctx->Global(), v8::String::New("sys"))
        );
        InitSyscalls(g_sysObj);

        // Run the bootloader, boot.js
        if (!GetBootLibPath(boot_path, sizeof(boot_path))) {
            fprintf(stderr, "%s: unable to determine boot path\n", g_execname);
            exit(1);
        }

        v8::Handle<v8::Value> val = ExecFile(boot_path);
        if (val.IsEmpty()) {
            exit(1);
        }
    }

    // Initialize the event loop and add our first thread
    
    g_runnableThreads.push_front(&app_thread);

    g_loop = ev_default_loop(EVFLAG_AUTO);

    // XXX: Dispatch needs a better name
    ev_prepare_init(&dispatch, DispatchCB);
    ev_prepare_start(g_loop, &dispatch);
    ev_unref(g_loop);

    ev_loop(g_loop, 0);
    ev_default_destroy();

    return 0;
}
