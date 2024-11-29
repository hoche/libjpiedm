cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)

project(JPIUtils
        LANGUAGES CXX
        VERSION 1.0)

set(CMAKE_BUILD_TYPE Debug)


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

