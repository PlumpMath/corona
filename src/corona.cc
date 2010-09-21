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
#include <coro.h>
#include "corona.h"
#include "v8-util.h"

class CoronaThread;

extern void InitSyscalls(v8::Handle<v8::Object> target);
static v8::Local<v8::Value> ExecFile(const char *);

char *g_execname = NULL;

static v8::Persistent<v8::Context> g_v8Ctx;
static v8::Persistent<v8::Object> g_sysObj;

static CoronaThread *g_currentThread = NULL;
static std::list<CoronaThread*> g_runnableThreads;
static std::list<CoronaThread*> g_blockedThreads;

class AppThread : public CoronaThread {
    public:
        AppThread(const std::string &str) : path_(str) {
        }

        void Run() {
            // We run first; there should be nobody else
            ASSERT(g_currentThread == NULL);
            g_currentThread = this;

            {
                v8::Locker lock;
                v8::Context::Scope ctx_scope(g_v8Ctx);
                v8::HandleScope scope;

                v8::Handle<v8::Value> val = ExecFile(path_.c_str());
                ASSERT(!val.IsEmpty());
            }

            CoronaThread::Run();
        }

    private:
        const std::string path_;
};

CoronaThread::CoronaThread() {
}

void CoronaThread::Run() {
    if (g_runnableThreads.empty()) {
        v8::internal::main_thread->Start();
        UNREACHABLE();
    }

    UNREACHABLE();
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

    AppThread app_thread(argv[1]);
    app_thread.Start();

    return 0;
}
