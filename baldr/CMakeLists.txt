file(GLOB_RECURSE BALDR_HEADER_FILES "*.hpp")

add_library(builder
    builder.cpp
    classifier.cpp
    docker.cpp
    config.cpp
    exec.cpp
)

target_link_libraries(builder PUBLIC utils Boost::headers nlohmann_json::nlohmann_json)
add_test_target(builder)

target_sources(builder PUBLIC
    FILE_SET HEADERS
    BASE_DIRS "${BALDR_PROJECT_ROOT}"
    FILES "${BALDR_HEADER_FILES}"
)

target_include_directories(builder PUBLIC
    $<BUILD_INTERFACE:${BALDR_PROJECT_ROOT}>
    $<INSTALL_INTERFACE:include>
)

add_executable(baldr main.cpp)
target_link_libraries(baldr PRIVATE builder Boost::program_options)

option(BALDR_STATIC_LINK "Fully statically link the baldr executable" OFF)

if(BALDR_STATIC_LINK)
    target_link_options(baldr PRIVATE -static -static-libgcc -static-libstdc++)
    target_compile_definitions(baldr PRIVATE BALDR_STATIC_LINK)
endif()

nova_target_build_info(baldr NAMESPACE baldr)

install(TARGETS baldr COMPONENT baldr)
