file(GLOB_RECURSE UTL_HEADER_FILES "*.hpp")

add_library(utils
    command.cpp
    tty.cpp
    rlog.cpp
)

target_sources(utils PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${BALDR_PROJECT_ROOT}"
    FILES "${UTL_HEADER_FILES}"
)

target_include_directories(utils PUBLIC
    $<BUILD_INTERFACE:${BALDR_PROJECT_ROOT}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(utils PUBLIC
    nova::nova
    fmt::fmt-header-only
    spdlog::spdlog_header_only
)

add_test_target(utils)

add_executable(example-rlog rlog.ex.cpp)
target_link_libraries(example-rlog PRIVATE utils)

install(TARGETS utils
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

add_executable(example-command command.ex.cpp)
target_link_libraries(example-command PRIVATE utils)
