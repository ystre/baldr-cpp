cmake_minimum_required(VERSION 3.24)

if(CMAKE_BINARY_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(FATAL_ERROR "Building in-source is not supported! Create a build dir and remove ${CMAKE_SOURCE_DIR}/CMakeCache.txt")
endif()

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BALDR_VERSION)
project("baldr" VERSION ${BALDR_VERSION} LANGUAGES CXX)
message(STATUS "Baldr version ${BALDR_VERSION}")

include(GNUInstallDirs)
include(dependencies.cmake)

include(novaCompilerWarnings)
include(novaCompilerSettings)
include(novaBuildInfo)

include(settings.cmake)

include(cmake/test.cmake)

set(BALDR_PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")

add_subdirectory(libutl)
add_subdirectory(baldr)

install(DIRECTORY res/ DESTINATION "${CMAKE_INSTALL_DATADIR}/baldr" COMPONENT baldr)

# The build tree has the layout of the install tree, so a built `baldr` finds them
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}")
file(CREATE_LINK "${BALDR_PROJECT_ROOT}/res" "${CMAKE_BINARY_DIR}/${CMAKE_INSTALL_DATADIR}/baldr" SYMBOLIC)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
