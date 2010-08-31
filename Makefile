.PHONY: all deps

all: build

build: deps

deps:
	mkdir -p deps/build
	cd deps/libev-3.9 && \
		./configure --prefix=$(shell pwd -P)/deps/build && \
		make install
	cd deps/libcoro && \
		make install INSTALLDIR=$(shell pwd -P)/deps/build

clean:
	rm -fr deps/build
	make -C deps/libev-3.9 clean
	make -C deps/libcoro clean
