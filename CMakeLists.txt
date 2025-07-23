cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

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
add_subdirectory(test/unit)
add_subdirectory(test/it)
