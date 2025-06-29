TOP ?= $(shell echo "`pwd`")

build:
	@if [ ! -d build ]; then mkdir build; fi
	cmake -S . -B build && cmake --build build -j
.PHONY: build

clean:
	@if [ -d build ]; then rm -rf $(TOP)/build; fi
.PHONY: clean

#test:
#	cd test && ./test.sh
#.PHONY: test

format:
	clang-format -i src/model/*.cpp \
			src/model/*.hpp
.PHONY: format
