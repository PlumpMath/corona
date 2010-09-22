LIBEV_VERS = 3.9
V8_VERS = 2.4.1

# All source files; used by ctags/cscope
SRC_FILES = $(shell find . -name '*.[ch]' -or -name '*.cc')

CFLAGS = -g -Wall -Werror 
CFLAGS += -DCORO_SJLJ -DDEBUG
CFLAGS += -Ideps/build/include
CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions
LDFLAGS = -Ldeps/build/lib
LDFLAGS += -lcoro -lev -lv8_g

.PHONY: all

all: build/benchd build/bench build/corona

build/benchd: build/obj/benchd.o deps/build/lib/libev.a deps/build/lib/libcoro.a
	$(CC) $(LDFLAGS) -o $@ $^

build/bench: build/obj/bench.o deps/build/lib/libev.a
	$(CC) $(LDFLAGS) -o $@ $^

build/corona: build/obj/corona.o build/obj/syscalls.o deps/build/lib/libev.a deps/build/lib/libv8_g.a
	$(CXX) $(LDFLAGS) -o $@ $^

build/obj/%.o: tools/%.c
	mkdir -p build/obj
	$(CC) $(CFLAGS) -o $@ -c $<

build/obj/%.o: src/%.cc
	mkdir -p build/obj
	$(CXX) $(CXXFLAGS) -o $@ -c $<

deps/build/lib/libev.a: $(shell find deps/libev-$(LIBEV_VERS) -name '*.[ch]')
	mkdir -p deps/build/lib deps/build/include
	cd deps/libev-$(LIBEV_VERS) && \
		(test -f config.status || ./configure --disable-shared --prefix=$(shell pwd -P)/deps/build) && \
		make install

deps/build/lib/libv8_g.a: $(shell find deps/v8-$(V8_VERS) -name '*.[ch]' -or -name '*.cc')
	mkdir -p deps/build/lib deps/build/include deps/build/include/v8
	cd deps/v8-$(V8_VERS) && \
		scons -j 4 visibility=default library=static mode=debug os=sigstack && \
		cp libv8_g.a $(shell pwd -P)/deps/build/lib && \
		cp include/*.h $(shell pwd -P)/deps/build/include && \
		cp src/*.h $(shell pwd -P)/deps/build/include/v8

deps/build/lib/libcoro.a: $(shell find deps/libcoro -name '*.[ch]')
	mkdir -p deps/build/lib deps/build/include
	cd deps/libcoro && \
		make install INSTALLDIR=$(shell pwd -P)/deps/build

tags: $(SRC_FILES)
	rm -f tags
	find . -name '*.[ch]' -or -name '*.cc' | ctags -L -
	find /usr/include -name '*.h' | ctags -L - -a
	find /opt/local/include -name '*.h' | ctags -L - -a

cscope.out: $(SRC_FILES)
	rm -f cscope.out
	find . -name '*.[ch]' -or -name '*.cc' | cscope -b -i -
	find /usr/include -name '*.h' | cscope -b -i -
	find /opt/local/include -name '*.h' | cscope -b -i -

clean:
	rm -fr deps/build
	rm -fr build
	make -C deps/libev-$(LIBEV_VERS) distclean clean
	make -C deps/libcoro clean
