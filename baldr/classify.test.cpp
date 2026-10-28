#include <baldr/classify.hpp>

#include <gtest/gtest.h>

using spdlog::level::err;
using spdlog::level::info;
using spdlog::level::warn;

TEST(Classify, CompilerError) {
    EXPECT_EQ(baldr::classify("src/a.cpp:48:24: error: 'tty' has not been declared"), err);
    EXPECT_EQ(baldr::classify("src/a.cpp:1:10: fatal error: x.hpp: No such file or directory"), err);
}

TEST(Classify, ColoredGccErrorIgnoresAnsi) {
    EXPECT_EQ(baldr::classify("\x1b[01m\x1b[Ka.cpp:4:2:\x1b[m\x1b[K \x1b[01;31m\x1b[Kerror: \x1b[m\x1b[Kbad"), err);
}

TEST(Classify, LinkerMakeNinjaAndTests) {
    EXPECT_EQ(baldr::classify("/usr/bin/ld: main.o: undefined reference to `foo()'"), err);
    EXPECT_EQ(baldr::classify("make[2]: *** [CMakeFiles/a.dir/build.make:76: a.o] Error 1"), err);
    EXPECT_EQ(baldr::classify("FAILED: CMakeFiles/a.dir/a.cpp.o"), err);
    EXPECT_EQ(baldr::classify("CMake Error at CMakeLists.txt:3 (add_executable):"), err);
    EXPECT_EQ(baldr::classify("[  FAILED  ] Suite.Case (0 ms)"), err);
    EXPECT_EQ(baldr::classify("75% tests passed, 1 tests failed out of 4"), err);
}

TEST(Classify, Warnings) {
    EXPECT_EQ(baldr::classify("a.cpp:3:9: warning: unused variable 'x' [-Wunused-variable]"), warn);
    EXPECT_EQ(baldr::classify("CMake Warning (dev) at CMakeLists.txt:1:"), warn);
}

TEST(Classify, EverythingElseIsInfo) {
    EXPECT_EQ(baldr::classify("[ 42%] Building CXX object a.cpp.o"), info);
    EXPECT_EQ(baldr::classify("-- Configuring done"), info);
    EXPECT_EQ(baldr::classify("make[2]: *** Waiting for unfinished jobs...."), info);
    EXPECT_EQ(baldr::classify(""), info);
}
