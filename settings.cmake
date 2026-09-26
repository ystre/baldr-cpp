set(CMAKE_CXX_STANDARD 23)

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -g")

if(COVERAGE)
    set(CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS} "-g -O0 -fno-inline -fprofile-arcs -ftest-coverage")
endif()