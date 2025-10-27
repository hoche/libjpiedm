TOP ?= $(shell echo "`pwd`")

.DEFAULT_GOAL := help

# Compiler detection - prefer clang, fallback to gcc
CLANG := $(shell command -v clang 2> /dev/null)
CLANGXX := $(shell command -v clang++ 2> /dev/null)
GCC := $(shell command -v gcc 2> /dev/null)
GXX := $(shell command -v g++ 2> /dev/null)

ifdef CLANGXX
    export CC := clang
    export CXX := clang++
    COMPILER_MSG := "Using clang/clang++"
else ifdef GXX
    export CC := gcc
    export CXX := g++
    COMPILER_MSG := "Using gcc/g++ (clang not found)"
else
    $(error "No suitable C++ compiler found. Please install clang++ or g++")
endif

# Sanity checks
CMAKE := $(shell command -v cmake 2> /dev/null)
CLANG_FORMAT := $(shell command -v clang-format 2> /dev/null)

build:
	@echo $(COMPILER_MSG)
	@if [ ! -d build ]; then mkdir build; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build && cmake --build build -j
.PHONY: build

build-verbose:
	@echo $(COMPILER_MSG)
	@if [ ! -d build ]; then mkdir build; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DDEBUG_VERBOSE=1 && cmake --build build -j
.PHONY: build-verbose

build-debug:
	@echo $(COMPILER_MSG)
	@if [ ! -d build ]; then mkdir build; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j
.PHONY: build-debug

# Sanitizer builds
build-asan:
	@echo $(COMPILER_MSG)
	@echo "Building with AddressSanitizer (ASan)"
	@if [ ! -d build-asan ]; then mkdir build-asan; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build-asan \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
		-DCMAKE_C_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" && \
	cmake --build build-asan -j
.PHONY: build-asan

build-tsan:
	@echo $(COMPILER_MSG)
	@echo "Building with ThreadSanitizer (TSan)"
	@if [ ! -d build-tsan ]; then mkdir build-tsan; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build-tsan \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
		-DCMAKE_C_FLAGS="-fsanitize=thread -fno-omit-frame-pointer -g" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" && \
	cmake --build build-tsan -j
.PHONY: build-tsan

build-ubsan:
	@echo $(COMPILER_MSG)
	@echo "Building with UndefinedBehaviorSanitizer (UBSan)"
	@if [ ! -d build-ubsan ]; then mkdir build-ubsan; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build-ubsan \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g" \
		-DCMAKE_C_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" && \
	cmake --build build-ubsan -j
.PHONY: build-ubsan

build-msan:
	@echo $(COMPILER_MSG)
	@echo "Building with MemorySanitizer (MSan) - clang only"
ifndef CLANGXX
	$(error "MemorySanitizer requires clang. Please install clang++")
endif
	@if [ ! -d build-msan ]; then mkdir build-msan; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build-msan \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_FLAGS="-fsanitize=memory -fno-omit-frame-pointer -g" \
		-DCMAKE_C_FLAGS="-fsanitize=memory -fno-omit-frame-pointer -g" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=memory" && \
	cmake --build build-msan -j
.PHONY: build-msan

build-lsan:
	@echo $(COMPILER_MSG)
	@echo "Building with LeakSanitizer (LSan)"
	@if [ ! -d build-lsan ]; then mkdir build-lsan; fi
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -f CMakeLists.txt ]; then echo "Error: CMakeLists.txt not found"; exit 1; fi
	cmake -S . -B build-lsan \
		-DCMAKE_BUILD_TYPE=Debug \
		-DCMAKE_CXX_FLAGS="-fsanitize=leak -fno-omit-frame-pointer -g" \
		-DCMAKE_C_FLAGS="-fsanitize=leak -fno-omit-frame-pointer -g" \
		-DCMAKE_EXE_LINKER_FLAGS="-fsanitize=leak" && \
	cmake --build build-lsan -j
.PHONY: build-lsan

build-all-sanitizers:
	@echo "Building with all sanitizers (ASan, TSan, UBSan, LSan, MSan if clang)"
	$(MAKE) build-asan
	$(MAKE) build-tsan
	$(MAKE) build-ubsan
	$(MAKE) build-lsan
ifdef CLANGXX
	$(MAKE) build-msan
endif
.PHONY: build-all-sanitizers

clean:
	@if [ -d build ]; then rm -rf $(TOP)/build; fi
	@if [ -d build-asan ]; then rm -rf $(TOP)/build-asan; fi
	@if [ -d build-tsan ]; then rm -rf $(TOP)/build-tsan; fi
	@if [ -d build-ubsan ]; then rm -rf $(TOP)/build-ubsan; fi
	@if [ -d build-lsan ]; then rm -rf $(TOP)/build-lsan; fi
	@if [ -d build-msan ]; then rm -rf $(TOP)/build-msan; fi
.PHONY: clean

test:
ifndef CMAKE
	$(error "cmake is not installed. Please install cmake >= 3.10")
endif
	@if [ ! -d build ]; then echo "Error: build directory not found. Run 'make build' first"; exit 1; fi
	@if [ ! -f build/parseedmlog ]; then echo "Error: parseedmlog not built. Run 'make build' first"; exit 1; fi
	cd build && ctest
.PHONY: test

# Sanitizer test targets
test-asan:
	@if [ ! -d build-asan ]; then echo "Error: build-asan directory not found. Run 'make build-asan' first"; exit 1; fi
	@if [ ! -f build-asan/parseedmlog ]; then echo "Error: parseedmlog not built with ASan. Run 'make build-asan' first"; exit 1; fi
	@echo "Running tests with AddressSanitizer..."
	cd build-asan && ctest
.PHONY: test-asan

test-tsan:
	@if [ ! -d build-tsan ]; then echo "Error: build-tsan directory not found. Run 'make build-tsan' first"; exit 1; fi
	@if [ ! -f build-tsan/parseedmlog ]; then echo "Error: parseedmlog not built with TSan. Run 'make build-tsan' first"; exit 1; fi
	@echo "Running tests with ThreadSanitizer..."
	cd build-tsan && ctest
.PHONY: test-tsan

test-ubsan:
	@if [ ! -d build-ubsan ]; then echo "Error: build-ubsan directory not found. Run 'make build-ubsan' first"; exit 1; fi
	@if [ ! -f build-ubsan/parseedmlog ]; then echo "Error: parseedmlog not built with UBSan. Run 'make build-ubsan' first"; exit 1; fi
	@echo "Running tests with UndefinedBehaviorSanitizer..."
	cd build-ubsan && ctest
.PHONY: test-ubsan

test-msan:
	@if [ ! -d build-msan ]; then echo "Error: build-msan directory not found. Run 'make build-msan' first"; exit 1; fi
	@if [ ! -f build-msan/parseedmlog ]; then echo "Error: parseedmlog not built with MSan. Run 'make build-msan' first"; exit 1; fi
	@echo "Running tests with MemorySanitizer..."
	cd build-msan && ctest
.PHONY: test-msan

test-lsan:
	@if [ ! -d build-lsan ]; then echo "Error: build-lsan directory not found. Run 'make build-lsan' first"; exit 1; fi
	@if [ ! -f build-lsan/parseedmlog ]; then echo "Error: parseedmlog not built with LSan. Run 'make build-lsan' first"; exit 1; fi
	@echo "Running tests with LeakSanitizer..."
	cd build-lsan && ctest
.PHONY: test-lsan

test-all-sanitizers:
	@echo "Running tests with all sanitizers..."
	$(MAKE) test-asan
	$(MAKE) test-tsan
	$(MAKE) test-ubsan
	$(MAKE) test-lsan
ifdef CLANGXX
	$(MAKE) test-msan
endif
.PHONY: test-all-sanitizers

format:
ifndef CLANG_FORMAT
	$(error "clang-format is not installed. Please install clang-format")
endif
	@if [ ! -d src/libjpiedm ]; then echo "Error: src/libjpiedm directory not found"; exit 1; fi
	@if [ ! -d src/parseedmlog ]; then echo "Error: src/parseedmlog directory not found"; exit 1; fi
	clang-format -i src/libjpiedm/*.cpp \
			src/libjpiedm/*.hpp \
			src/parseedmlog/*.cpp
.PHONY: format

help:
	@echo "libjpiedm Makefile - Available targets:"
	@echo ""
	@echo "Build targets:"
	@echo "  build              - Standard release build (default compiler: clang -> gcc)"
	@echo "  build-debug        - Debug build with symbols"
	@echo "  build-verbose      - Debug build with verbose output"
	@echo ""
	@echo "Sanitizer builds (for debugging memory/thread issues):"
	@echo "  build-asan         - Build with AddressSanitizer (memory errors)"
	@echo "  build-tsan         - Build with ThreadSanitizer (data races)"
	@echo "  build-ubsan        - Build with UndefinedBehaviorSanitizer"
	@echo "  build-lsan         - Build with LeakSanitizer (memory leaks)"
	@echo "  build-msan         - Build with MemorySanitizer (clang only)"
	@echo "  build-all-sanitizers - Build with all sanitizers"
	@echo ""
	@echo "Test targets:"
	@echo "  test               - Run tests with standard build"
	@echo "  test-asan          - Run tests with AddressSanitizer"
	@echo "  test-tsan          - Run tests with ThreadSanitizer"
	@echo "  test-ubsan         - Run tests with UndefinedBehaviorSanitizer"
	@echo "  test-lsan          - Run tests with LeakSanitizer"
	@echo "  test-msan          - Run tests with MemorySanitizer"
	@echo "  test-all-sanitizers - Run tests with all sanitizers"
	@echo ""
	@echo "Utility targets:"
	@echo "  clean              - Remove all build directories"
	@echo "  format             - Format source code with clang-format"
	@echo "  help               - Show this help message"
	@echo ""
	@echo "Compiler preference: clang/clang++ (fallback to gcc/g++)"
	@echo "Current compiler: $(COMPILER_MSG)"
.PHONY: help
