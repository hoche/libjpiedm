TOP ?= $(shell echo "`pwd`")

build:
	@if [ ! -d build ]; then mkdir build; fi
	cmake -S . -B build && cmake --build build -j
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
			src/sample_app/*.cpp
.PHONY: format
