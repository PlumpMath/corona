#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <v8.h>
#include "corona.h"

#define SET_ERRNO(target, e) \
    (target)->Set( \
        v8::String::New(#e), \
        v8::Integer::New(e), \
        (v8::PropertyAttribute) (v8::ReadOnly | v8::DontDelete) \
    );

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

    SET_ERRNO(target, EPERM);
    SET_ERRNO(target, ENOENT);
    SET_ERRNO(target, ESRCH);
    SET_ERRNO(target, EINTR);
    SET_ERRNO(target, EIO);
    SET_ERRNO(target, ENXIO);
    SET_ERRNO(target, E2BIG);
    SET_ERRNO(target, ENOEXEC);
    SET_ERRNO(target, EBADF);
    SET_ERRNO(target, ECHILD);
    SET_ERRNO(target, EDEADLK);
    SET_ERRNO(target, ENOMEM);
    SET_ERRNO(target, EACCES);
    SET_ERRNO(target, EFAULT);
#ifdef ENOTBLK
    SET_ERRNO(target, ENOTBLK);
#endif
    SET_ERRNO(target, EBUSY);
    SET_ERRNO(target, EEXIST);
    SET_ERRNO(target, EXDEV);
    SET_ERRNO(target, ENODEV);
    SET_ERRNO(target, ENOTDIR);
    SET_ERRNO(target, EISDIR);
    SET_ERRNO(target, EINVAL);
    SET_ERRNO(target, ENFILE);
    SET_ERRNO(target, EMFILE);
    SET_ERRNO(target, ENOTTY);
    SET_ERRNO(target, ETXTBSY);
    SET_ERRNO(target, EFBIG);
    SET_ERRNO(target, ENOSPC);
    SET_ERRNO(target, ESPIPE);
    SET_ERRNO(target, EROFS);
    SET_ERRNO(target, EMLINK);
    SET_ERRNO(target, EPIPE);
    SET_ERRNO(target, EDOM);
    SET_ERRNO(target, ERANGE);
    SET_ERRNO(target, EAGAIN);
    SET_ERRNO(target, EWOULDBLOCK);
    SET_ERRNO(target, EINPROGRESS);
    SET_ERRNO(target, EALREADY);
    SET_ERRNO(target, ENOTSOCK);
    SET_ERRNO(target, EDESTADDRREQ);
    SET_ERRNO(target, EMSGSIZE);
    SET_ERRNO(target, EPROTOTYPE);
    SET_ERRNO(target, ENOPROTOOPT);
    SET_ERRNO(target, EPROTONOSUPPORT);
#ifdef ESOCKTNOSUPPORT
    SET_ERRNO(target, ESOCKTNOSUPPORT);
#endif
    SET_ERRNO(target, ENOTSUP);
#ifdef EOPNOTSUPP
    SET_ERRNO(target, EOPNOTSUPP);
#endif
#ifdef EPFNOSUPPORT
    SET_ERRNO(target, EPFNOSUPPORT);
#endif
    SET_ERRNO(target, EAFNOSUPPORT);
    SET_ERRNO(target, EADDRINUSE);
    SET_ERRNO(target, EADDRNOTAVAIL);
    SET_ERRNO(target, ENETDOWN);
    SET_ERRNO(target, ENETUNREACH);
    SET_ERRNO(target, ENETRESET);
    SET_ERRNO(target, ECONNABORTED);
    SET_ERRNO(target, ECONNRESET);
    SET_ERRNO(target, ENOBUFS);
    SET_ERRNO(target, EISCONN);
    SET_ERRNO(target, ENOTCONN);
#ifdef ESHUTDOWN
    SET_ERRNO(target, ESHUTDOWN);
#endif
#ifdef ETOOMANYREFS
    SET_ERRNO(target, ETOOMANYREFS);
#endif
    SET_ERRNO(target, ETIMEDOUT);
    SET_ERRNO(target, ECONNREFUSED);
    SET_ERRNO(target, ELOOP);
    SET_ERRNO(target, ENAMETOOLONG);
#ifdef EHOSTDOWN
    SET_ERRNO(target, EHOSTDOWN);
#endif
    SET_ERRNO(target, EHOSTUNREACH);
    SET_ERRNO(target, ENOTEMPTY);
#ifdef EPROCLIM
    SET_ERRNO(target, EPROCLIM);
#endif
#ifdef EUSERS
    SET_ERRNO(target, EUSERS);
#endif
    SET_ERRNO(target, EDQUOT);
    SET_ERRNO(target, ESTALE);
#ifdef EREMOTE
    SET_ERRNO(target, EREMOTE);
#endif
#ifdef EBADRPC
    SET_ERRNO(target, EBADRPC);
#endif
#ifdef ERPCMISMATCH
    SET_ERRNO(target, ERPCMISMATCH);
#endif
#ifdef EPROGUNAVAIL
    SET_ERRNO(target, EPROGUNAVAIL);
#endif
#ifdef EPROGMISMATCH
    SET_ERRNO(target, EPROGMISMATCH);
#endif
#ifdef EPROCUNAVAIL
    SET_ERRNO(target, EPROCUNAVAIL);
#endif
    SET_ERRNO(target, ENOLCK);
    SET_ERRNO(target, ENOSYS);
#ifdef EFTYPE
    SET_ERRNO(target, EFTYPE);
#endif
#ifdef EAUTH
    SET_ERRNO(target, EAUTH);
#endif
#ifdef ENEEDAUTH
    SET_ERRNO(target, ENEEDAUTH);
#endif
#ifdef EPWROFF
    SET_ERRNO(target, EPWROFF);
#endif
#ifdef EDEVERR
    SET_ERRNO(target, EDEVERR);
#endif
    SET_ERRNO(target, EOVERFLOW);
#ifdef EBADEXEC
    SET_ERRNO(target, EBADEXEC);
#endif
#ifdef EBADARCH
    SET_ERRNO(target, EBADARCH);
#endif
#ifdef ESHLIBVERS
    SET_ERRNO(target, ESHLIBVERS);
#endif
#ifdef EBADMACHO
    SET_ERRNO(target, EBADMACHO);
#endif
    SET_ERRNO(target, ECANCELED);
    SET_ERRNO(target, EIDRM);
    SET_ERRNO(target, ENOMSG);
    SET_ERRNO(target, EILSEQ);
#ifdef ENOATTR
    SET_ERRNO(target, ENOATTR);
#endif
    SET_ERRNO(target, EBADMSG);
    SET_ERRNO(target, EMULTIHOP);
    SET_ERRNO(target, ENODATA);
    SET_ERRNO(target, ENOLINK);
    SET_ERRNO(target, ENOSR);
    SET_ERRNO(target, ENOSTR);
    SET_ERRNO(target, EPROTO);
    SET_ERRNO(target, ETIME);
#ifdef EOPNOTSUPP
    SET_ERRNO(target, EOPNOTSUPP);
#endif
    SET_ERRNO(target, ENOPOLICY);
#ifdef ELAST
    SET_ERRNO(target, ELAST);
#endif
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
