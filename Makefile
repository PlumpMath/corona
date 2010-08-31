# XXX: Unfortunately, we're not able to statically link (gcc -static) when
# 	   building bench_ev. If we do, linking fails with
#
# 	   	ld: library not found for -lcrt0.o
#
# XXX: Use automake/autoconf for libcoro, and/or include its source inline.

.PHONY: all deps

all: build

build: deps build/bench_ev

build/bench_ev: src/bench_ev.c
	mkdir -p build
	gcc -Wall -Werror -Ideps/build/include -Ldeps/build/lib -lcoro -lev -o $@ $<

deps:
	mkdir -p deps/build
	cd deps/libev-3.9 && \
		./configure --prefix=$(shell pwd -P)/deps/build && \
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
	make -C deps/libev-3.9 clean
	make -C deps/libcoro clean
