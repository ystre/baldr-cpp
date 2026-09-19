cmake_minimum_required(VERSION 3.24)

if(CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

set(BALDR_VERSION "0.4.1")
project("baldr" VERSION ${BALDR_VERSION} LANGUAGES CXX)
message(STATUS "Baldr version ${BALDR_VERSION}")

find_package(nova REQUIRED)
include(GNUInstallDirs)
include(dependencies.cmake)

include(novaCompilerWarnings)
include(novaCompilerSettings)

include(settings.cmake)

include(cmake/git-version.cmake)
include(cmake/test.cmake)

set(BALDR_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")

add_subdirectory(libutl)
add_subdirectory(baldr)

if(BUILD_TESTING)
    # Root tests if any
endif()
