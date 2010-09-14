// Sample code to test V8 with alternate stacks
//
// This code works by initializing V8 in main() before executing a script on a
// different stack. We create this new stack by using sigaltstack(2) and firing
// off a signal to get there. We then use setjmp(2 to save our position before
// returning from the signal handler. At this point, we have a fresh stack that
// we can longjmp(2) to (we do this setjmp(2)/longjmp(2) dance to avoid
// executing V8 code in a signal handler). We do this and run some V8 code,
// which blows up.

#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <setjmp.h>
#include <v8.h>
#include <v8/globals.h>
#include <v8/checks.h>
#include <v8/allocation.h>
#include <v8/utils.h>
#include <v8/list.h>
#include <v8/platform.h>

using namespace v8;

Persistent<Context> ctx;

class TestThread : public v8::internal::Thread {
    public:
        void Run() {
            printf("starting v8 in callback ...\n");

            Locker lock;
            Context::Scope ctx_scope(ctx);
            HandleScope scope;
            TryCatch try_catch;

            Local<String> scriptContents = String::New("foo();");
            Local<Script> script = Script::Compile(
                scriptContents,
                String::New("foo.js")
            );
            assert(!script.IsEmpty());

            Local<Value> result = script->Run();
            assert(!result.IsEmpty());
            assert(!try_catch.HasCaught());
        }
};

// Dummy binding just to show that we're executing something
static Handle<Value>
Foo(const Arguments &args) {
    printf("foo!\n");
    return Undefined();
}

int
main(int argc, char *argv[]) {
    {
        Locker l;

        ctx = Context::New();
        Context::Scope ctx_scope(ctx);
        HandleScope scope;

        ctx->Global()->Set(
            String::NewSymbol("foo"),
            FunctionTemplate::New(Foo)->GetFunction()
        );
    }

    TestThread tt;
    tt.Initialize(v8::internal::ThreadHandle::INVALID);
    tt.Start();

    return 0;
}
