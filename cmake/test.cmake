include(CTest)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)
endif()

function(add_test_target MODULE_NAME)
    if(NOT BUILD_TESTING)
        return()
    endif()

    set(target "test-${MODULE_NAME}")
    file(GLOB_RECURSE TEST_SOURCES "*.test.cpp")
    add_executable("${target}" ${TEST_SOURCES})

    target_link_libraries(${target} PRIVATE
        ${MODULE_NAME}
        nova::nova

        GTest::gtest_main
        GTest::gtest
        GTest::gmock
    )

    gtest_discover_tests(
        ${target}
        DISCOVERY_MODE PRE_TEST
    )
endfunction()
