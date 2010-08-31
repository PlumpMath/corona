LIBEV_VERSION=3.9
LIBEV_DIR=libev-$(LIBEV_VERSION)

.PHONY: all deps

all: build

build: deps

deps:
	mkdir deps/build
	cd deps/$(LIBEV_DIR) && \
		./configure --prefix=$(shell pwd -P)/deps/build && \
		make install

clean:
	rm -fr deps/build
