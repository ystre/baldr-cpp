cmake_minimum_required(VERSION 3.20)
project(baldr-cmake-testproject CXX)

# Minimal CMake-based project, used to manually exercise `baldr build`/`baldr
# run` against a CMake build system.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(src)

# Surface BALDR_TEST_DEFINE (if given via `baldr build -D...`) to the
# executable, so manual verification of `-D` forwarding can print it.
if(DEFINED BALDR_TEST_DEFINE)
    target_compile_definitions(hello PRIVATE BALDR_TEST_DEFINE="${BALDR_TEST_DEFINE}")
endif()
