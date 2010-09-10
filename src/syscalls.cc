#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <v8.h>
#include "corona.h"

static v8::Handle<v8::Value>
GetErrno(v8::Local<v8::String> name, const v8::AccessorInfo &info) {
    return v8::Integer::New(errno);
}

static void
InitErrno(const v8::Handle<v8::Object> target) {
    target->SetAccessor(
        v8::String::NewSymbol("errno"),
        GetErrno
    );
}

// write(2)
static v8::Handle<v8::Value>
Write(const v8::Arguments &args) {
    v8::HandleScope scope;

    int32_t fd = -1;
    char *buf = NULL;
    size_t buf_len = 0;
    int err;

    V8_ARG_VALUE_FD(fd, args, 0);
    V8_ARG_VALUE_UTF8(buf, args, 1);
    buf_len = args[1]->ToString()->Utf8Length();

    err = write(fd, buf, buf_len);

    return scope.Close(v8::Integer::New(err));
}

// Set system call functions on the given target object
void InitSyscalls(const v8::Handle<v8::Object> target) {
    InitErrno(target);

    target->Set(
        v8::String::NewSymbol("write"),
        v8::FunctionTemplate::New(Write)->GetFunction()
    );
}
