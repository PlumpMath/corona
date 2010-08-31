# A simplistic Makefile to build libcoro.a.

.PHONY: all install

all: libcoro.a

libcoro.a: coro.c coro.h
	gcc -D_XOPEN_SOURCE -DCORO_UCONTEXT -c -o coro.o coro.c
	libtool -static -o libcoro.a coro.o

install: libcoro.a
	cp libcoro.a $(INSTALLDIR)/lib
	cp coro.h $(INSTALLDIR)/include

clean:
	rm -f coro.o libcoro.a
