#ifndef __corona_corona_h__
#define __corona_corona_h__

#include <v8.h>

// Helper macros for argument parsing

#define V8_ARG_EXISTS(args, index) \
    do { \
        if ((args).Length() < ((index) + 1)) { \
            return v8::ThrowException(v8::Exception::TypeError( \
                v8::String::New( \
                    "Missing required argument at index " #index \
                ) \
            )); \
        } \
    } while (0)

#define V8_ARG_TYPE(args, index, type) \
    do { \
        if (!(args)[(index)]->Is##type()) { \
            return v8::ThrowException(v8::Exception::TypeError( \
                v8::String::New( \
                    "Argument at index " #index " is not a " #type \
                ) \
            )); \
        } \
    } while (0)

#define V8_ARG_VALUE(lval, args, index, type) \
    do { \
        V8_ARG_EXISTS(args, index); \
        V8_ARG_TYPE(args, index, type); \
        (lval) = (args)[(index)]->type##Value(); \
    } while (0)

#define V8_ARG_VALUE_UTF8(lval, args, index) \
    do { \
        V8_ARG_EXISTS(args, index); \
        V8_ARG_TYPE(args, index, String); \
        (lval) = *v8::String::Utf8Value((args)[(index)]->ToString()); \
    } while (0)

#define V8_ARG_VALUE_FD(lval, args, index) \
    do { \
        V8_ARG_VALUE(lval, args, index, Int32); \
        if ((lval) < 0) { \
            return v8::ThrowException(v8::Exception::TypeError( \
                v8::String::New( \
                    "Argument at index " #index " not a valid file descriptor" \
                ) \
            )); \
        } \
    } while (0)

// Helper functions for other V8 interactions

static inline v8::Handle<v8::Object>
CreateNamespace(v8::Handle<v8::Object> target, v8::Handle<v8::String> name) {
    v8::Local<v8::Object> o =
        (v8::FunctionTemplate::New())->GetFunction()->NewInstance();
    target->Set(name, o);

    return o;
}

// Misc globals

extern char *g_execname;

#endif /* __corona_corona_h__ */
