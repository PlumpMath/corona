
.PHONY: all deps

all: deps build/benchd build/bench

build/benchd: src/benchd.c deps build
	gcc -DCORO_SJLJ -g -Wall -Werror -Ideps/build/include -Ldeps/build/lib -lcoro -lev -o $@ $<

build/bench: src/bench.c deps build
	gcc -DCORO_SJLJ -g -Wall -Werror -Ideps/build/include -Ldeps/build/lib -lcoro -lev -o $@ $<

build:
	mkdir -p build

deps:
	mkdir -p deps/build
	cd deps/libev-3.9 && \
		(test -f config.status || ./configure --disable-shared --prefix=$(shell pwd -P)/deps/build) && \
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
	rm -fr build
	make -C deps/libev-3.9 distclean clean
	make -C deps/libcoro clean
