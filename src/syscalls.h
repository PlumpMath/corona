#ifndef __corona_syscall_h__
#define __corona_syscall_h__

#include <v8.h>

void InitSyscalls(v8::Handle<v8::Object> target);

#endif /* __corona_syscall_h__ */
