#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <string.h>
#include <libgen.h>
#include <ctype.h>
#include <errno.h>
#include <v8.h>
#include "corona.h"
#include "syscalls.h"

static char *g_execname;

static v8::Persistent<v8::Context> g_v8Ctx;
static v8::Persistent<v8::Object> g_sysObj;

// atexit() handler; tears down all global state
static void
ExitCB(void) {
    g_v8Ctx.Dispose();
    v8::V8::Dispose();
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
    v8::String::Utf8Value msgStr(msg->Get());
    v8::String::Utf8Value resourceStr(msg->GetScriptResourceName());

    fprintf(
        fp,
        "%s: %s at %s:%d,%d\n",
            g_execname, *msgStr, *resourceStr, msg->GetLineNumber(),
            msg->GetStartColumn()
    );
}

// Execute the script held in a file of the given name
//
// The script is run in the current context and its result value is returned.
// On error, the process is exited.
static v8::Handle<v8::Value>
ExecMainScript(const char *fname) {
    v8::HandleScope scope;
    v8::Handle<v8::Value> scriptResult;
    v8::TryCatch try_catch;
    int fd = -1;
    char *buf = NULL;
    size_t buf_sz = 4;
    size_t buf_off = 0;
    ssize_t nread = 0;

    assert(scriptResult.IsEmpty());

    // Open our script file
    fd = open(fname, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "ExecMainScript: open failed: %s\n", strerror(errno));
        exit(1);
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

    if (nread < 0) {
        free(buf);
        fprintf(stderr, "ExecMainScript: read failed: %s\n", strerror(errno));
        exit(1);
    }

    v8::Local<v8::Script> script = v8::Script::Compile(
        v8::String::New(buf, buf_off),
        v8::String::New(fname)
    );
    free(buf);

    if (script.IsEmpty()) {
        assert(try_catch.HasCaught());
        LogException(stderr, try_catch);

        exit(1);
    }

    scriptResult = script->Run();
    if (scriptResult.IsEmpty()) {
        assert(try_catch.HasCaught());
        LogException(stderr, try_catch);

        exit(1);
    }

    return scope.Close(scriptResult);
}

// TODO: Get script payload from commandline (stdin by default)
//
// TODO: Parse arguments using FlagList::SetFlagsFromCommandLine(); use '--' to
//       delimit V8 options from corona options
int
main(int argc, char *argv[]) {
    g_execname = basename(argv[0]);

    v8::V8::Initialize();
    v8::V8::SetFatalErrorHandler(FatalErrorCB);

    v8::HandleScope scope;

    g_v8Ctx = v8::Context::New();
    v8::Context::Scope v8CtxScope(g_v8Ctx);

    atexit(ExitCB);

    v8::Local<v8::Object> globalObj = g_v8Ctx->Global();
    g_sysObj = v8::Persistent<v8::Object>::New(
        CreateNamespace(globalObj, v8::String::New("sys"))
    );
    InitSyscalls(g_sysObj);

    v8::Handle<v8::Value> val = ExecMainScript(argv[1]);

    return 0;
}
