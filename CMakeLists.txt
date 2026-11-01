cmake_minimum_required(VERSION 3.24)

if(CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BALDR_VERSION)
project("baldr" VERSION ${BALDR_VERSION} LANGUAGES CXX)
message(STATUS "Baldr version ${BALDR_VERSION}")

include(GNUInstallDirs)

# dependencies.cmake's first find_package() call triggers conan's dependency
# provider, which (absent a forced -DCONAN_HOST_PROFILE) auto-detects a host
# profile from the current CMAKE_CXX_STANDARD. Give it the real value early;
# settings.cmake (included below, after novaCompilerSettings) still sets the
# authoritative value used for the actual build.
set(CMAKE_CXX_STANDARD 23)

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
