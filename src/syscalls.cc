#include <errno.h>
#include <stddef.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <v8.h>
#include "corona.h"

// Set a constant numerical value on the given target
#define SET_CONST(target, e) \
    (target)->Set( \
        v8::String::NewSymbol(#e), \
        v8::Integer::New(e), \
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete) \
    )

// The list of PROTO_* names that we want to fill in using getprotoent(3)
static const char *proto_names[] = {
    "TCP",
    "UDP",
    NULL
};

// Accessor for 'errno' property
static v8::Handle<v8::Value>
GetErrno(v8::Local<v8::String> name, const v8::AccessorInfo &info) {
    return v8::Integer::New(errno);
}

// Set errno constants in the target namespace
static void
InitErrno(const v8::Handle<v8::Object> target) {
    target->SetAccessor(
        v8::String::NewSymbol("errno"),
        GetErrno
    );

    SET_CONST(target, EPERM);
    SET_CONST(target, ENOENT);
    SET_CONST(target, ESRCH);
    SET_CONST(target, EINTR);
    SET_CONST(target, EIO);
    SET_CONST(target, ENXIO);
    SET_CONST(target, E2BIG);
    SET_CONST(target, ENOEXEC);
    SET_CONST(target, EBADF);
    SET_CONST(target, ECHILD);
    SET_CONST(target, EDEADLK);
    SET_CONST(target, ENOMEM);
    SET_CONST(target, EACCES);
    SET_CONST(target, EFAULT);
#ifdef ENOTBLK
    SET_CONST(target, ENOTBLK);
#endif
    SET_CONST(target, EBUSY);
    SET_CONST(target, EEXIST);
    SET_CONST(target, EXDEV);
    SET_CONST(target, ENODEV);
    SET_CONST(target, ENOTDIR);
    SET_CONST(target, EISDIR);
    SET_CONST(target, EINVAL);
    SET_CONST(target, ENFILE);
    SET_CONST(target, EMFILE);
    SET_CONST(target, ENOTTY);
    SET_CONST(target, ETXTBSY);
    SET_CONST(target, EFBIG);
    SET_CONST(target, ENOSPC);
    SET_CONST(target, ESPIPE);
    SET_CONST(target, EROFS);
    SET_CONST(target, EMLINK);
    SET_CONST(target, EPIPE);
    SET_CONST(target, EDOM);
    SET_CONST(target, ERANGE);
    SET_CONST(target, EAGAIN);
    SET_CONST(target, EWOULDBLOCK);
    SET_CONST(target, EINPROGRESS);
    SET_CONST(target, EALREADY);
    SET_CONST(target, ENOTSOCK);
    SET_CONST(target, EDESTADDRREQ);
    SET_CONST(target, EMSGSIZE);
    SET_CONST(target, EPROTOTYPE);
    SET_CONST(target, ENOPROTOOPT);
    SET_CONST(target, EPROTONOSUPPORT);
#ifdef ESOCKTNOSUPPORT
    SET_CONST(target, ESOCKTNOSUPPORT);
#endif
    SET_CONST(target, ENOTSUP);
#ifdef EOPNOTSUPP
    SET_CONST(target, EOPNOTSUPP);
#endif
#ifdef EPFNOSUPPORT
    SET_CONST(target, EPFNOSUPPORT);
#endif
    SET_CONST(target, EAFNOSUPPORT);
    SET_CONST(target, EADDRINUSE);
    SET_CONST(target, EADDRNOTAVAIL);
    SET_CONST(target, ENETDOWN);
    SET_CONST(target, ENETUNREACH);
    SET_CONST(target, ENETRESET);
    SET_CONST(target, ECONNABORTED);
    SET_CONST(target, ECONNRESET);
    SET_CONST(target, ENOBUFS);
    SET_CONST(target, EISCONN);
    SET_CONST(target, ENOTCONN);
#ifdef ESHUTDOWN
    SET_CONST(target, ESHUTDOWN);
#endif
#ifdef ETOOMANYREFS
    SET_CONST(target, ETOOMANYREFS);
#endif
    SET_CONST(target, ETIMEDOUT);
    SET_CONST(target, ECONNREFUSED);
    SET_CONST(target, ELOOP);
    SET_CONST(target, ENAMETOOLONG);
#ifdef EHOSTDOWN
    SET_CONST(target, EHOSTDOWN);
#endif
    SET_CONST(target, EHOSTUNREACH);
    SET_CONST(target, ENOTEMPTY);
#ifdef EPROCLIM
    SET_CONST(target, EPROCLIM);
#endif
#ifdef EUSERS
    SET_CONST(target, EUSERS);
#endif
    SET_CONST(target, EDQUOT);
    SET_CONST(target, ESTALE);
#ifdef EREMOTE
    SET_CONST(target, EREMOTE);
#endif
#ifdef EBADRPC
    SET_CONST(target, EBADRPC);
#endif
#ifdef ERPCMISMATCH
    SET_CONST(target, ERPCMISMATCH);
#endif
#ifdef EPROGUNAVAIL
    SET_CONST(target, EPROGUNAVAIL);
#endif
#ifdef EPROGMISMATCH
    SET_CONST(target, EPROGMISMATCH);
#endif
#ifdef EPROCUNAVAIL
    SET_CONST(target, EPROCUNAVAIL);
#endif
    SET_CONST(target, ENOLCK);
    SET_CONST(target, ENOSYS);
#ifdef EFTYPE
    SET_CONST(target, EFTYPE);
#endif
#ifdef EAUTH
    SET_CONST(target, EAUTH);
#endif
#ifdef ENEEDAUTH
    SET_CONST(target, ENEEDAUTH);
#endif
#ifdef EPWROFF
    SET_CONST(target, EPWROFF);
#endif
#ifdef EDEVERR
    SET_CONST(target, EDEVERR);
#endif
    SET_CONST(target, EOVERFLOW);
#ifdef EBADEXEC
    SET_CONST(target, EBADEXEC);
#endif
#ifdef EBADARCH
    SET_CONST(target, EBADARCH);
#endif
#ifdef ESHLIBVERS
    SET_CONST(target, ESHLIBVERS);
#endif
#ifdef EBADMACHO
    SET_CONST(target, EBADMACHO);
#endif
    SET_CONST(target, ECANCELED);
    SET_CONST(target, EIDRM);
    SET_CONST(target, ENOMSG);
    SET_CONST(target, EILSEQ);
#ifdef ENOATTR
    SET_CONST(target, ENOATTR);
#endif
    SET_CONST(target, EBADMSG);
    SET_CONST(target, EMULTIHOP);
    SET_CONST(target, ENODATA);
    SET_CONST(target, ENOLINK);
    SET_CONST(target, ENOSR);
    SET_CONST(target, ENOSTR);
    SET_CONST(target, EPROTO);
    SET_CONST(target, ETIME);
#ifdef EOPNOTSUPP
    SET_CONST(target, EOPNOTSUPP);
#endif
    SET_CONST(target, ENOPOLICY);
#ifdef ELAST
    SET_CONST(target, ELAST);
#endif
}

// Set fcntl-related constants in the target namespace
static void
InitFcntl(const v8::Handle<v8::Object> target) {
    SET_CONST(target, F_GETFL);
    SET_CONST(target, F_SETFL);

    SET_CONST(target, O_NONBLOCK);
    SET_CONST(target, O_APPEND);
    SET_CONST(target, O_ASYNC);
}

// Set networking-related constants in the target namespace
static void
InitNet(const v8::Handle<v8::Object> target) {
    char buf[512];
    int i;

    // AF_*
    SET_CONST(target, AF_UNIX);
    SET_CONST(target, AF_INET);

    // SOCK_*
    SET_CONST(target, SOCK_STREAM);
    SET_CONST(target, SOCK_DGRAM);

    // PROTO_*
    for (i = 0; proto_names[i]; i++) {
        struct protoent *pent;
        
        if (!(pent = getprotobyname(proto_names[i]))) {
            fprintf(
                stderr,
                "%s: warning: could not resolve protocol %s\n",
                    g_execname, proto_names[i]
            );
            continue;
        }

        sprintf(buf, "PROTO_%s", proto_names[i]);

        target->Set(
            v8::String::NewSymbol(buf),
            v8::Integer::New(pent->p_proto),
            (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete)
        );
    }
}

// write(2)
//
// <nbytes> = write(<fd>, <string>)
static v8::Handle<v8::Value>
Write(const v8::Arguments &args) {
    v8::HandleScope scope;

    int32_t fd = -1;
    char *buf = NULL;
    size_t buf_len = 0;
    int err = -1;

    V8_ARG_VALUE_FD(fd, args, 0);
    V8_ARG_VALUE_UTF8(buf, args, 1);
    buf_len = args[1]->ToString()->Utf8Length();

    err = write(fd, buf, buf_len);
    return scope.Close(v8::Integer::New(err));
}

// socket(2)
//
// <fd> = socket(<af-value>, <pf-value>, <proto-value>)
static v8::Handle<v8::Value>
Socket(const v8::Arguments &args) {
    v8::HandleScope scope;

    int domain = -1;
    int type = -1;
    int proto = -1;
    int fd = -1;

    V8_ARG_VALUE(domain, args, 0, Int32);
    V8_ARG_VALUE(type, args, 1, Int32);
    V8_ARG_VALUE(proto, args, 2, Int32);

    fd = socket(domain, type, proto);
    return scope.Close(v8::Integer::New(fd));
}

// bind(2)
//
// <err> = bind(<fd>, <port>[, <address>])
// <err> = bind(<fd>, <path>)
static v8::Handle<v8::Value>
Bind(const v8::Arguments &args) {
    v8::HandleScope scope;

    int fd = -1;
    int err = -1;
    struct sockaddr *addr = NULL;
    struct sockaddr_in addr_in;
    struct sockaddr_un addr_un;

    V8_ARG_VALUE_FD(fd, args, 0);

    V8_ARG_EXISTS(args, 1);
    if (args[1]->IsNumber()) {
        int port = -1;
        char *addr_str = NULL;

        port = args[1]->Int32Value();
        if (port < 0) {
            return v8::ThrowException(v8::Exception::TypeError(FormatString(
                "Port argument must be a positive integer: %d", port
            )));
        }

        if (args.Length() >= 3) {
            if (!args[2]->IsString()) {
                return v8::ThrowException(v8::Exception::TypeError(
                    v8::String::New(
                        "Address argument must be a string"
                    )
                ));
            }

            V8_ARG_VALUE_UTF8(addr_str, args, 2);
        }

        bzero(&addr_in, sizeof(addr_in));
        addr_in.sin_len = sizeof(addr_in);
        addr_in.sin_family = AF_INET;
        addr_in.sin_port = htons(port);
        if (!inet_aton(addr_str, &addr_in.sin_addr)) {
            return v8::ThrowException(v8::Exception::TypeError(FormatString(
                "Invalid address argument specified: %s", addr_str
            )));
        }

        addr = (struct sockaddr*) &addr_in;
    } else if (args[1]->IsString()) {
        char *path;

        V8_ARG_VALUE_UTF8(path, args, 1);

        bzero(&addr_un, sizeof(addr_un));
        addr_un.sun_len = offsetof(struct sockaddr_un, sun_path) + strlen(path);
        addr_un.sun_family = AF_UNIX;
        strcpy(addr_un.sun_path, path);

        addr = (struct sockaddr*) &addr_un;
    } else {
        return v8::ThrowException(v8::Exception::TypeError(v8::String::New(
            "Argument at index 1 must be either a port or a path"
        )));
    }

    err = bind(fd, addr, addr->sa_len);
    return scope.Close(v8::Integer::New(err));
}

// listen(2)
//
// <err> = listen(<fd>, <backlog>)
static v8::Handle<v8::Value>
Listen(const v8::Arguments &args) {
    v8::HandleScope scope;

    int fd = -1;
    int backlog = -1;
    int err;

    V8_ARG_VALUE_FD(fd, args, 0);
    V8_ARG_VALUE(backlog, args, 1, Int32);

    err = listen(fd, backlog);
    return scope.Close(v8::Integer::New(err));
}

// fcntl(2)
//
// <val> = fcntl(<fd>, <cmd>, ...)
static v8::Handle<v8::Value>
Fcntl(const v8::Arguments &args) {
    v8::HandleScope scope;

    int fd = -1;
    int cmd = -1;
    int flags = 0;
    int err = -1;

    V8_ARG_VALUE_FD(fd, args, 0);
    V8_ARG_VALUE(cmd, args, 1, Int32);

    // Read the remaining values and execute the system call
    switch (cmd) {
    case F_GETFL:
        err = fcntl(fd, cmd);
        break;

    case F_SETFL:
        V8_ARG_VALUE(flags, args, 2, Int32);
        err = fcntl(fd, cmd, flags);
        break;

    default:
        return v8::ThrowException(v8::Exception::Error(FormatString(
            "Unknown command specified: %d", cmd
        )));
    }

    return scope.Close(v8::Integer::New(err));
}

// Set system call functions on the given target object
void InitSyscalls(const v8::Handle<v8::Object> target) {
    InitErrno(target);
    InitFcntl(target);
    InitNet(target);

    target->Set(
        v8::String::NewSymbol("write"),
        v8::FunctionTemplate::New(Write)->GetFunction(),
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete)
    );
    target->Set(
        v8::String::NewSymbol("socket"),
        v8::FunctionTemplate::New(Socket)->GetFunction(),
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete)
    );
    target->Set(
        v8::String::NewSymbol("bind"),
        v8::FunctionTemplate::New(Bind)->GetFunction(),
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete)
    );
    target->Set(
        v8::String::NewSymbol("listen"),
        v8::FunctionTemplate::New(Listen)->GetFunction(),
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete)
    );
    target->Set(
        v8::String::NewSymbol("fcntl"),
        v8::FunctionTemplate::New(Fcntl)->GetFunction(),
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete)
    );
}
