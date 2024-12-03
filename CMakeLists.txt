cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(jpiedm
        LANGUAGES CXX
        VERSION 1.0)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS YES CACHE BOOL "Export all symbols")

message("cmake module path: ${CMAKE_MODULE_PATH}")
message("current cmakelist directory: ${CMAKE_CURRENT_LIST_DIR}")
message("project source directory: ${PROJECT_SOURCE_DIR}")
message("project binary directory: ${PROJECT_BINARY_DIR}")

# Set compilation flags
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# add the binary directory to the search path for include files
include_directories("${PROJECT_BINARY_DIR}")

add_subdirectory(src)

