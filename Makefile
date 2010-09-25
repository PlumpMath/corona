LIBEV_VERS = 3.9
V8_VERS = 2.4.1

# All source files; used by ctags/cscope
SRC_FILES = $(shell find . -name '*.[ch]' -or -name '*.cc')

# Dependencies
LIBEV_PATH = deps/build/lib/libev.a
LIBV8_PATH = deps/build/lib/libv8_g.a
LIB_PATHS = $(LIBEV_PATH) $(LIBV8_PATH)

# V8 settings to build using SCons
LIBV8_SCONS_SETTINGS = visibility=default library=static mode=debug os=sigstack

CFLAGS = -g -Wall -Werror 
CFLAGS += -DCORO_SJLJ -DDEBUG -D_DARWIN_UNLIMITED_SELECT
CFLAGS += -Ideps/build/include
CXXFLAGS = $(CFLAGS) -fno-rtti -fno-exceptions
LDFLAGS = -Ldeps/build/lib
LDFLAGS += -lev -lv8_g

.PHONY: all

all: build/corona build/tcp

build/corona: build/obj/corona.o build/obj/syscalls.o build/obj/sched.o
	$(CXX) $(LDFLAGS) -o $@ $^

build/tcp: build/obj/tcp.o
	$(CC) $(LDFLAGS) -o $@ $^

build/obj/%.o: src/%.cc $(LIB_PATHS)
	@mkdir -p build/obj
	$(CXX) $(CXXFLAGS) -o $@ -c $<

build/obj/%.o: bench/%.c $(LIB_PATHS)
	@mkdir -p build/obj
	$(CC) $(CFLAGS) -o $@ -c $<

$(LIBEV_PATH): $(shell find deps/libev-$(LIBEV_VERS) -name '*.[ch]')
	mkdir -p deps/build/lib deps/build/include
	cd deps/libev-$(LIBEV_VERS) && \
		(test -f config.status || \
			CFLAGS=-D_DARWIN_UNLIMITED_SELECT ./configure --disable-shared --prefix=$(shell pwd -P)/deps/build) && \
		make install

$(LIBV8_PATH): $(shell find deps/v8-$(V8_VERS) -name '*.[ch]' -or -name '*.cc')
	mkdir -p deps/build/lib deps/build/include deps/build/include/v8
	cd deps/v8-$(V8_VERS) && \
		scons -j 4 $(LIBV8_SCONS_SETTINGS)  && \
		cp libv8_g.a $(shell pwd -P)/deps/build/lib && \
		cp include/*.h $(shell pwd -P)/deps/build/include && \
		cp src/*.h $(shell pwd -P)/deps/build/include/v8

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
	cd deps/libev-$(LIBEV_VERS) && \
		[[ ! -f Makefile ]] || make clean distclean
	cd deps/v8-$(V8_VERS) && \
		scons -j 4 -c $(LIBV8_SCONS_SETTINGS) 
