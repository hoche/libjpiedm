cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

# Compiler detection - prefer clang, fallback to gcc
# This must be set before project() call
if(NOT DEFINED ENV{CC} AND NOT DEFINED ENV{CXX})
    # Check for clang first
    execute_process(
        COMMAND which clang++
        OUTPUT_VARIABLE CLANGXX_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND which clang
        OUTPUT_VARIABLE CLANG_PATH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(CLANGXX_PATH AND CLANG_PATH)
        set(CMAKE_C_COMPILER ${CLANG_PATH})
        set(CMAKE_CXX_COMPILER ${CLANGXX_PATH})
        message(STATUS "Detected and using clang/clang++ compiler")
    else()
        # Check for gcc as fallback
        execute_process(
            COMMAND which g++
            OUTPUT_VARIABLE GXX_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        execute_process(
            COMMAND which gcc
            OUTPUT_VARIABLE GCC_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(GXX_PATH AND GCC_PATH)
            set(CMAKE_C_COMPILER ${GCC_PATH})
            set(CMAKE_CXX_COMPILER ${GXX_PATH})
            message(STATUS "Using gcc/g++ compiler (clang not found)")
        else()
            message(WARNING "No suitable C++ compiler found. CMake will try to find one.")
        endif()
    endif()
endif()

project(jpiedm
        LANGUAGES CXX
        VERSION 1.0)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS YES CACHE BOOL "Export all symbols")

# Set compilation flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# libjpiedm library
add_library(jpiedm
    src/libjpiedm/FlightFile.cpp
    src/libjpiedm/FlightIterator.cpp
    src/libjpiedm/FileHeaders.cpp
    src/libjpiedm/Flight.cpp
    src/libjpiedm/Metadata.cpp
    src/libjpiedm/Metrics.cpp
)

if(DEBUG_VERBOSE)
  target_compile_definitions(jpiedm PUBLIC DEBUG_FLIGHTS=1 DEBUG_FLIGHT_HEADERS=1 DEBUG_FLIGHT_RECORD=1)
endif()


target_include_directories(jpiedm
    PUBLIC src/libjpiedm
)


# parseedmlog executable
add_executable(parseedmlog
    src/parseedmlog/main.cpp
)

target_include_directories(parseedmlog
    PUBLIC src/libjpiedm src/parseedmlog
)

if(WIN32)
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)
	target_sources(parseedmlog
        PRIVATE src/parseedmlog/getopt.cpp
	)
	target_link_libraries(parseedmlog
		PUBLIC Ws2_32.lib PRIVATE jpiedm
	)
else()
	target_link_libraries(parseedmlog
		PRIVATE jpiedm
	)
endif()

# testing
enable_testing()
add_subdirectory(tests/unit)
add_subdirectory(tests/it)

# ==============================================================================
# Sanitizer and Static Analysis Targets
# ==============================================================================

# Function to create sanitizer build targets
function(add_sanitizer_target TARGET_NAME SANITIZER_NAME SANITIZER_FLAGS)
    add_custom_target(${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} -E echo "Building with ${SANITIZER_NAME}..."
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND}
            "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
            "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DCMAKE_CXX_FLAGS=-fsanitize=${SANITIZER_FLAGS} -fno-omit-frame-pointer -g -O1"
            "-DCMAKE_C_FLAGS=-fsanitize=${SANITIZER_FLAGS} -fno-omit-frame-pointer -g -O1"
            "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=${SANITIZER_FLAGS}"
            -S ${CMAKE_SOURCE_DIR}
            -B ${CMAKE_BINARY_DIR}/${TARGET_NAME}
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/${TARGET_NAME} --target unit_tests
        COMMAND ${CMAKE_COMMAND} -E echo "Running tests with ${SANITIZER_NAME}..."
        COMMAND cd ${CMAKE_BINARY_DIR}/${TARGET_NAME} && ./unit_tests
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running ${SANITIZER_NAME}"
        VERBATIM
    )
endfunction()

# AddressSanitizer - detects memory errors (use-after-free, buffer overflows, etc.)
add_sanitizer_target(asan "AddressSanitizer (ASan)" "address")

# UndefinedBehaviorSanitizer - detects undefined behavior
add_sanitizer_target(ubsan "UndefinedBehaviorSanitizer (UBSan)" "undefined")

# ThreadSanitizer - detects data races and deadlocks
# Uses suppression file to filter known false positives from GDAL library
add_custom_target(tsan
    COMMAND ${CMAKE_COMMAND} -E echo "Building with ThreadSanitizer..."
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/tsan
    COMMAND ${CMAKE_COMMAND}
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_CXX_FLAGS=-fsanitize=thread -fno-omit-frame-pointer -g -O1"
        "-DCMAKE_C_FLAGS=-fsanitize=thread -fno-omit-frame-pointer -g -O1"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=thread"
        -S ${CMAKE_SOURCE_DIR}
        -B ${CMAKE_BINARY_DIR}/tsan
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/tsan --target unit_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running tests with ThreadSanitizer..."
    COMMAND ${CMAKE_SOURCE_DIR}/run_tsan.sh ${CMAKE_BINARY_DIR}/tsan/unit_tests
    COMMENT "Running ThreadSanitizer"
)

# MemorySanitizer - detects uninitialized memory reads (Clang only)
# Note: MSan requires Clang and ALL dependencies compiled with MSan
# This is typically not feasible in standard development environments
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_custom_target(msan
        COMMAND ${CMAKE_COMMAND} -E echo "=== MemorySanitizer MSan Information ==="
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "MSan requires ALL code including system libraries to be"
        COMMAND ${CMAKE_COMMAND} -E echo "compiled with -fsanitize=memory. In most environments,"
        COMMAND ${CMAKE_COMMAND} -E echo "system libraries like GDAL, PROJ, libpng are NOT compiled"
        COMMAND ${CMAKE_COMMAND} -E echo "with MSan, which prevents MSan from running successfully."
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "This is a known limitation of MSan, not a bug in SPLAT."
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "For this reason, the msan target is informational only."
        COMMAND ${CMAKE_COMMAND} -E echo "See TESTING.md for full details on MSan limitations."
        COMMAND ${CMAKE_COMMAND} -E echo ""
        COMMAND ${CMAKE_COMMAND} -E echo "If you have an MSan-instrumented environment, build with:"
        COMMAND ${CMAKE_COMMAND} -E echo "  CC=clang CXX=clang++ cmake -B build-msan"
        COMMAND ${CMAKE_COMMAND} -E echo "    -DCMAKE_CXX_FLAGS=-fsanitize=memory"
        COMMAND ${CMAKE_COMMAND} -E echo "    -DCMAKE_C_FLAGS=-fsanitize=memory"
        COMMAND ${CMAKE_COMMAND} -E echo "    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=memory"
        COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-msan"
        COMMAND ${CMAKE_COMMAND} -E echo "  cd build-msan"
        COMMAND ${CMAKE_COMMAND} -E echo "  ./unit_tests"
        COMMENT "MSan information - not running, see output"
    )
else()
    add_custom_target(msan
        COMMAND ${CMAKE_COMMAND} -E echo "MemorySanitizer requires Clang compiler"
        COMMAND ${CMAKE_COMMAND} -E echo "Current compiler: ${CMAKE_CXX_COMPILER_ID}"
        COMMAND ${CMAKE_COMMAND} -E echo "To use MSan, reconfigure with: CC=clang CXX=clang++ cmake .."
        COMMAND ${CMAKE_COMMAND} -E false
        COMMENT "MSan not available with current compiler"
    )
endif()

# LeakSanitizer - detects memory leaks (can be run standalone or with ASan)
add_custom_target(lsan
    COMMAND ${CMAKE_COMMAND} -E echo "Running LeakSanitizer..."
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/lsan
    COMMAND ${CMAKE_COMMAND}
        "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}"
        "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DCMAKE_CXX_FLAGS=-fsanitize=leak -fno-omit-frame-pointer -g -O1"
        "-DCMAKE_C_FLAGS=-fsanitize=leak -fno-omit-frame-pointer -g -O1"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=leak"
        -S ${CMAKE_SOURCE_DIR}
        -B ${CMAKE_BINARY_DIR}/lsan
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}/lsan --target unit_tests
    COMMAND ${CMAKE_COMMAND} -E echo "Running tests with LeakSanitizer..."
    COMMAND cd ${CMAKE_BINARY_DIR}/lsan && ASAN_OPTIONS=detect_leaks=1 ./unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running LeakSanitizer"
    VERBATIM
)

# Valgrind - memory error detector and profiler
add_custom_target(valgrind
    COMMAND ${CMAKE_COMMAND} -E echo "Running Valgrind..."
    COMMAND valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes
        --verbose --error-exitcode=1 ${CMAKE_BINARY_DIR}/unit_tests
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Valgrind memory checker"
    VERBATIM
)

# Valgrind with memcheck only (faster)
add_custom_target(valgrind-quick
    COMMAND ${CMAKE_COMMAND} -E echo "Running Valgrind (quick)..."
    COMMAND valgrind --leak-check=yes --error-exitcode=1 ${CMAKE_BINARY_DIR}/unit_tests
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Valgrind (quick mode)"
    VERBATIM
)

# Clang-Tidy - static analysis
add_custom_target(clang-tidy
    COMMAND ${CMAKE_COMMAND} -E echo "Running Clang-Tidy..."
    COMMAND clang-tidy
        ${SPLAT_SOURCES}
        -p ${CMAKE_BINARY_DIR}
        --checks='-*,bugprone-*,clang-analyzer-*,cppcoreguidelines-*,modernize-*,performance-*,readability-*'
        --warnings-as-errors=''
        --header-filter='.*'
    DEPENDS unit_tests
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running Clang-Tidy static analysis"
    VERBATIM
)

# CppCheck - static analysis
add_custom_target(cppcheck
    COMMAND ${CMAKE_COMMAND} -E echo "Running CppCheck..."
    COMMAND cppcheck
        --enable=all
        --inconclusive
        --std=c++17
        --suppress=missingIncludeSystem
        --suppress=unmatchedSuppression
        --inline-suppr
        --error-exitcode=1
        --quiet
        -I src
        src/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running CppCheck static analysis"
    VERBATIM
)

# CppCheck with verbose output
add_custom_target(cppcheck-verbose
    COMMAND ${CMAKE_COMMAND} -E echo "Running CppCheck (verbose)..."
    COMMAND cppcheck
        --enable=all
        --inconclusive
        --std=c++17
        --suppress=missingIncludeSystem
        -I src
        src/
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running CppCheck static analysis (verbose)"
    VERBATIM
)

# Combined sanitizers target (runs all sanitizers sequentially)
# Note: MSan only runs on Clang
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_custom_target(sanitizers
        COMMAND ${CMAKE_COMMAND} -E echo "Running all sanitizers..."
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target asan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ubsan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target tsan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target lsan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target msan
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running all sanitizers sequentially (including MSan)"
        VERBATIM
    )
else()
    add_custom_target(sanitizers
        COMMAND ${CMAKE_COMMAND} -E echo "Running all sanitizers (MSan skipped - requires Clang)..."
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target asan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ubsan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target tsan
        COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target lsan
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Running all sanitizers sequentially (MSan not available)"
        VERBATIM
    )
endif()

# Combined static analysis target
add_custom_target(analyze
    COMMAND ${CMAKE_COMMAND} -E echo "Running all static analysis tools..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target cppcheck
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target clang-tidy
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running all static analysis tools"
    VERBATIM
)

# All checks target (sanitizers + static analysis + valgrind)
add_custom_target(check-all
    COMMAND ${CMAKE_COMMAND} -E echo "Running comprehensive checks..."
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target sanitizers
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target analyze
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target valgrind
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Running all checks (sanitizers, static analysis, valgrind)"
    VERBATIM
)
