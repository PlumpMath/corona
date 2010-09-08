#include <stdlib.h>
#include <v8.h>

using namespace v8;

static Persistent<Context> v8Ctx;

void exit_cb(void) {
    v8Ctx.Dispose();
    V8::Dispose();
}

int main(int argc, char *argv[]) {
    V8::Initialize();

    HandleScope scope;

    Persistent<Context> v8Ctx = Context::New();
    Context::Scope v8CtxScope(v8Ctx);

    atexit(exit_cb);

    return 0;
}
