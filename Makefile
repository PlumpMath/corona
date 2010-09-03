# XXX: Unfortunately, we're not able to statically link (gcc -static) when
# 	   building bench_ev. If we do, linking fails with
#
# 	   	ld: library not found for -lcrt0.o
#
# XXX: Use automake/autoconf for libcoro, and/or include its source inline.

.PHONY: all deps

all: deps build/ev build/client

build/ev: src/ev.c deps build
	gcc -g -Wall -Werror -Ideps/build/include -Ldeps/build/lib -lcoro -lev -o $@ $<

build/client: src/client.c deps build
	gcc -g -Wall -Werror -Ideps/build/include -Ldeps/build/lib -lcoro -lev -o $@ $<

build:
	mkdir -p build

deps:
	mkdir -p deps/build
	cd deps/libev-3.9 && \
		(test -f config.status || ./configure --prefix=$(shell pwd -P)/deps/build) && \
		make install
	cd deps/libcoro && \
		make install INSTALLDIR=$(shell pwd -P)/deps/build

tags:
	rm -f tags
	find . -name '*.[ch]' | ctags -L -
	find /usr/include | ctags -L - -a
	find /opt/local/include | ctags -L - -a

clean:
	rm -fr deps/build
	make -C deps/libev-3.9 distclean clean
	make -C deps/libcoro clean
