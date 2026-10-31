#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <baldr/config.hpp>

#include <libnova/error.hpp>
#include <libnova/dyn_value.hpp>

#include <string_view>
#include <tuple>

using namespace std::string_view_literals;

namespace {

constexpr auto global_config = R"yaml(
debugger: lldb
debugger-args: [--batch]
build-type: Release

env:
  GLOBAL_ENV: global
  CUSTOM_ENV: something

cmake:
  definitions:
    GLOBAL_DEFINE: global
    CUSTOM_DEFINE: something

docker:
  mount-baldr: false
)yaml"sv;

constexpr auto local_config = R"yaml(

env:
  CUSTOM_ENV: something else
  LOCAL_ENV: local

cmake:
  definitions:
    CUSTOM_DEFINE: something else
    LOCAL_DEFINE: local
)yaml"sv;

[[nodiscard]] auto yaml(std::string_view text) -> nova::dyn_value {
    return nova::dyn_value::parse(text, nova::dyn_value::format_e::yaml).value();
}

[[nodiscard]] auto merged(std::string_view base, std::string_view overlay) -> baldr::config {
    return baldr::parse(nova::dyn_value::merge(yaml(base), yaml(overlay)));
}

TEST(config, Defaults) {
    const auto res = baldr::parse(nova::dyn_value::map({ }));
    EXPECT_EQ(res.debugger, "gdb");
    EXPECT_EQ(res.debugger_args, (std::vector<std::string>{ "--args" }));
    EXPECT_EQ(res.build_type, "Debug");
    EXPECT_TRUE(res.cmake_defines.empty());
    EXPECT_TRUE(res.env.empty());
    EXPECT_TRUE(res.docker_mount_baldr);
    EXPECT_FALSE(res.docker_baldr_path.has_value());
}

TEST(config, ProjectLocalFileIsParsed) {
    const auto res = baldr::parse(yaml(local_config));
    EXPECT_EQ(res.debugger, "gdb");
    EXPECT_EQ(res.build_type, "Debug");
    EXPECT_EQ(res.cmake_defines.at("CUSTOM_DEFINE"), "something else");
    EXPECT_EQ(res.cmake_defines.at("LOCAL_DEFINE"), "local");
    EXPECT_EQ(res.env.at("CUSTOM_ENV"), "something else");
    EXPECT_EQ(res.env.at("LOCAL_ENV"), "local");
}

TEST(config, ConfigMerging) {
    const auto res = merged(global_config, local_config);
    EXPECT_EQ(res.debugger, "lldb");
    EXPECT_EQ(res.debugger_args, (std::vector<std::string>{ "--batch" }));
    EXPECT_EQ(res.build_type, "Release");
    EXPECT_FALSE(res.docker_mount_baldr);
    EXPECT_EQ(res.cmake_defines.at("GLOBAL_DEFINE"), "global");
    EXPECT_EQ(res.cmake_defines.at("CUSTOM_DEFINE"), "something else");
    EXPECT_EQ(res.cmake_defines.at("LOCAL_DEFINE"), "local");
    EXPECT_EQ(res.env.at("GLOBAL_ENV"), "global");
    EXPECT_EQ(res.env.at("CUSTOM_ENV"), "something else");
    EXPECT_EQ(res.env.at("LOCAL_ENV"), "local");
}

TEST(config, ListsAreReplaced) {
    const auto res = merged(global_config, "debugger-args: [-q, --args]\n");
    EXPECT_EQ(res.debugger_args, (std::vector<std::string>{ "-q", "--args" }));
}

TEST(config, NullResetsToDefault) {
    const auto res = merged(global_config, "build-type:\n");
    EXPECT_EQ(res.build_type, "Debug");
}

TEST(config, NullDeletesInheritedKey) {
    const auto res = merged(global_config, "env:\n  GLOBAL_ENV:\n");
    EXPECT_FALSE(res.env.contains("GLOBAL_ENV"));
    EXPECT_EQ(res.env.at("CUSTOM_ENV"), "something");
}

TEST(config, DockerBaldrPathIsOptional) {
    EXPECT_FALSE(baldr::parse(yaml("docker:\n  mount-baldr: true\n")).docker_baldr_path.has_value());
    EXPECT_EQ(baldr::parse(yaml("docker:\n  baldr-path: /opt/baldr\n")).docker_baldr_path, "/opt/baldr");
}

TEST(config, ScalarsReadAsStrings) {
    const auto res = baldr::parse(yaml(R"yaml(
cmake:
  definitions:
    CMAKE_CXX_STANDARD: 23
    BUILD_TESTING: ON
    ENABLE_X: true
)yaml"));
    EXPECT_EQ(res.cmake_defines.at("CMAKE_CXX_STANDARD"), "23");
    EXPECT_EQ(res.cmake_defines.at("BUILD_TESTING"), "ON");
    EXPECT_EQ(res.cmake_defines.at("ENABLE_X"), "true");
}

TEST(config, Negative_ErrorNamesTheKey) {
    EXPECT_THAT(
        [] { std::ignore = baldr::parse(yaml("env:\n  NESTED:\n    key: value\n")); },
        testing::ThrowsMessage<nova::exception>(testing::HasSubstr("Config `env.NESTED`: expected a scalar, got map"))
    );

    EXPECT_THAT(
        [] { std::ignore = baldr::parse(yaml("debugger-args: --args\n")); },
        testing::ThrowsMessage<nova::exception>(testing::HasSubstr("Config `debugger-args`: expected seq of string, got string"))
    );
}

TEST(config, Negative_WrongTypeIsError) {
    EXPECT_THROW(std::ignore = baldr::parse(yaml("env: [a, b]\n")), nova::exception);
    EXPECT_THROW(std::ignore = baldr::parse(yaml("debugger-args: --args\n")), nova::exception);
    EXPECT_THROW(std::ignore = baldr::parse(yaml("docker:\n  mount-baldr: yes\n")), nova::exception);
    EXPECT_THROW(std::ignore = baldr::parse(yaml("env:\n  NESTED:\n    key: value\n")), nova::exception);
}

} // namespace
