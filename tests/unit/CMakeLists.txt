# Unit tests with Google Test
include(FetchContent)

# Download and make Google Test available
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(googletest)

# Enable testing
enable_testing()
include(GoogleTest)

# Single combined test executable
add_executable(unit_tests
    metadata_test.cpp
    metrics_test.cpp
    fileheaders_test.cpp
    flight_test.cpp
    flightfile_test.cpp
)

target_link_libraries(unit_tests
    PRIVATE
    jpiedm
    GTest::gtest_main
)

target_include_directories(unit_tests
    PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libjpiedm
)

# Discover and register all tests from the combined executable
gtest_discover_tests(unit_tests)
