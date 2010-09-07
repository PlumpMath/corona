LIBEV_VERS = 3.9
V8_VERS = 2.4.1

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
	cd deps/libev-$(LIBEV_VERS) && \
		(test -f config.status || ./configure --disable-shared --prefix=$(shell pwd -P)/deps/build) && \
		make install
	cd deps/libcoro && \
		make install INSTALLDIR=$(shell pwd -P)/deps/build
	cd deps/v8-$(V8_VERS) && \
		scons -j 4 visibility=default library=static mode=debug && \
		cp libv8_g.a $(shell pwd -P)/deps/build/lib && \
		cp include/*.h $(shell pwd -P)/deps/build/include

tags:
	rm -f tags
	find . -name '*.[ch]' | ctags -L -
	find /usr/include | ctags -L - -a
	find /opt/local/include | ctags -L - -a

clean:
	rm -fr deps/build
	rm -fr build
	make -C deps/libev-$(LIBEV_VERS) distclean clean
	make -C deps/libcoro clean
