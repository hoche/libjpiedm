TOP ?= $(shell echo "`pwd`")

build:
	@if [ ! -d build ]; then mkdir build; fi
	cmake -S . -B build && cmake --build build -j
.PHONY: build

build-verbose:
	@if [ ! -d build ]; then mkdir build; fi
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDEBUG_VERBOSE=1 && cmake --build build -j
.PHONY: build

build-debug:
	@if [ ! -d build ]; then mkdir build; fi
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j
.PHONY: build

clean:
	@if [ -d build ]; then rm -rf $(TOP)/build; fi
.PHONY: clean

test:
	cd build && ctest
.PHONY: test

format:
	clang-format -i src/libjpiedm/*.cpp \
			src/libjpiedm/*.hpp \
			src/parseedmlog/*.cpp
.PHONY: format
