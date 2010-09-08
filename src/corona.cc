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

static v8::Persistent<v8::Context> g_v8Ctx;
static char *g_execname;

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

// This is a test
static v8::Handle<v8::Value>
LogCB(const v8::Arguments &args) {
    v8::HandleScope scope;

    fprintf(stderr, "LogCB()\n");

    return scope.Close(v8::Undefined());
}

// Execute the script held in a file of the given name
//
// The script is run in the current context and its result value is returned.
// On error, an empty handle is returned.
static v8::Handle<v8::Value>
ExecScript(const char *fname) {
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
        fprintf(stderr, "ExecScript: open failed: %s\n", strerror(errno));

        return scope.Close(scriptResult);
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
        fprintf(stderr, "ExecScript: read failed: %s\n", strerror(errno));

        return scope.Close(scriptResult);
    }

    v8::Local<v8::Script> script = v8::Script::Compile(
        v8::String::New(buf, buf_off),
        v8::String::New(fname)
    );
    if (script.IsEmpty()) {
        assert(try_catch.HasCaught());

        v8::Local<v8::Message> msg = try_catch.Message();
        v8::String::Utf8Value msgStr(msg->Get());

        fprintf(
            stderr,
            "ExecScript: %s at line %d, column %d\n",
                *msgStr, msg->GetLineNumber(), msg->GetStartColumn()
        );

        free(buf);

        return scope.Close(scriptResult);
    }

    // XXX: Need a TryCatch around this
    scriptResult = script->Run();
    assert(!scriptResult.IsEmpty());

    free(buf);

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

    v8::Local<v8::Object> globalObj = g_v8Ctx->Global();
    globalObj->Set(
        v8::String::NewSymbol("log"),
        v8::FunctionTemplate::New(LogCB)->GetFunction()
    );

    v8::Handle<v8::Value> val = ExecScript(argv[1]);
    if (val.IsEmpty()) {
        fprintf(stderr, "%s: ExecScript failed!\n", argv[0]);
        exit(1);
    }

    atexit(ExitCB);

    return 0;
}
