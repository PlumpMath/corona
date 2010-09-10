LIBEV_VERS = 3.9
V8_VERS = 2.4.1

CFLAGS = -g -Wall -Werror 
CFLAGS += -DCORO_SJLJ
CFLAGS += -Ideps/build/include
CXXFLAGS = $(CFLAGS)
LDFLAGS = -Ldeps/build/lib
LDFLAGS += -lcoro -lev -lv8_g

.PHONY: all deps

all: deps build/benchd build/bench build/corona

build/benchd: build/obj/benchd.o
	$(CC) $(LDFLAGS) -o $@ $^

build/bench: build/obj/bench.o
	$(CC) $(LDFLAGS) -o $@ $^

build/corona: build/obj/corona.o build/obj/syscalls.o
	$(CXX) $(LDFLAGS) -o $@ $^

build/obj/%.o: src/%.c build/obj deps
	$(CC) $(CFLAGS) -o $@ -c $<

build/obj/%.o: src/%.cc build/obj deps
	$(CXX) $(CXXFLAGS) -o $@ -c $<

build/obj:
	mkdir -p build/obj

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
	find . -name '*.[ch]' -or -name '*.cc' | ctags -L -
	find /usr/include | ctags -L - -a
	find /opt/local/include | ctags -L - -a

clean:
	rm -fr deps/build
	rm -fr build
	make -C deps/libev-$(LIBEV_VERS) distclean clean
	make -C deps/libcoro clean
