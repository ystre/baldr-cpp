#include <gtest/gtest.h>

#include <baldr/config.hpp>

#include <libnova/error.hpp>

#include <string_view>
#include <tuple>

using namespace std::string_view_literals;

namespace {

constexpr auto global_config = R"yaml(
debugger: gdb
debugger-args:
build-type: Release

env:
  GLOBAL_ENV: global
  CUSTOM_ENV: something

cmake:
  definitions:
    GLOBAL_DEFINE: global
    CUSTOM_DEFINE: something
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

TEST(config, Negative_MalformedYamlIsError) {
    const auto res = baldr::load("debugger: [invalid\n"sv, baldr::config{});
    ASSERT_FALSE(res.has_value());
    EXPECT_FALSE(res.error().message.empty());
}

TEST(config, MissingFileReturnsDefaults) {
    const baldr::config res;
    EXPECT_EQ(res.debugger, "gdb");
    EXPECT_EQ(res.build_type, "Debug");
    EXPECT_TRUE(res.cmake_defines.empty());
    EXPECT_TRUE(res.env.empty());
    EXPECT_TRUE(res.docker_mount_baldr);
    EXPECT_FALSE(res.docker_baldr_path.has_value());
}

TEST(config, ProjectLocalFileIsLoaded) {
    const auto res = baldr::load(local_config, baldr::config{});
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->debugger, "gdb");
    EXPECT_EQ(res->build_type, "Debug");
    EXPECT_EQ(res->cmake_defines.at("CUSTOM_DEFINE"), "something else");
    EXPECT_EQ(res->cmake_defines.at("LOCAL_DEFINE"), "local");
    EXPECT_EQ(res->env.at("CUSTOM_ENV"), "something else");
    EXPECT_EQ(res->env.at("LOCAL_ENV"), "local");
}

TEST(config, BuildTypeOverride) {
    const auto res = baldr::load("build-type: Release\n"sv, baldr::config{});
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->build_type, "Release");
}

TEST(config, ConfigMerging) {
    const auto global = baldr::load(global_config, baldr::config{});
    ASSERT_TRUE(global.has_value());

    const auto res = baldr::load(local_config, *global);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->debugger, "gdb");
    EXPECT_EQ(res->build_type, "Release");
    EXPECT_EQ(res->cmake_defines.at("GLOBAL_DEFINE"), "global");
    EXPECT_EQ(res->cmake_defines.at("CUSTOM_DEFINE"), "something else");
    EXPECT_EQ(res->cmake_defines.at("LOCAL_DEFINE"), "local");
    EXPECT_EQ(res->env.at("GLOBAL_ENV"), "global");
    EXPECT_EQ(res->env.at("CUSTOM_ENV"), "something else");
    EXPECT_EQ(res->env.at("LOCAL_ENV"), "local");
}

constexpr auto batch_config = R"yaml(
batch:
  env:
    ARTIFACT_DIR: .tmp
    REPORT_DIR: ${ARTIFACT_DIR}/reports

  stages:
    clean:
      cmd: "./scripts/clean.sh"
    install:
      parameters:
        - build_type
      cmd: "baldr build -t baldr $build_type -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR"

  scenarios:
    dev:
      - clean
      - install @build-type=Debug
)yaml"sv;

TEST(config, BatchEnvInterpolation) {
    const auto res = baldr::load(batch_config, baldr::config{});
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->batch.env.at("ARTIFACT_DIR"), ".tmp");
    EXPECT_EQ(res->batch.env.at("REPORT_DIR"), ".tmp/reports");
}

TEST(config, BatchStagesParsed) {
    const auto res = baldr::load(batch_config, baldr::config{});
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(res->batch.stages.contains("clean"));
    EXPECT_EQ(res->batch.stages.at("clean").cmd, "./scripts/clean.sh");
    EXPECT_TRUE(res->batch.stages.at("clean").parameters.empty());

    ASSERT_TRUE(res->batch.stages.contains("install"));
    EXPECT_EQ(res->batch.stages.at("install").parameters, (std::vector<std::string>{ "build_type" }));
}

TEST(config, BatchScenariosParsed) {
    const auto res = baldr::load(batch_config, baldr::config{});
    ASSERT_TRUE(res.has_value());
    ASSERT_TRUE(res->batch.scenarios.contains("dev"));

    const auto& steps = res->batch.scenarios.at("dev");
    ASSERT_EQ(steps.size(), 2);
    EXPECT_EQ(steps[0].stage, "clean");
    EXPECT_TRUE(steps[0].overrides.empty());
    EXPECT_EQ(steps[1].stage, "install");
    EXPECT_EQ(steps[1].overrides.at("build_type"), "Debug");
}

TEST(config, ParseStepNormalizesHyphenToUnderscore) {
    const auto step = baldr::parse_step("install @build-type=Release");
    EXPECT_EQ(step.stage, "install");
    EXPECT_EQ(step.overrides.at("build_type"), "Release");
}

TEST(config, ParseStepNoOverrides) {
    const auto step = baldr::parse_step("clean");
    EXPECT_EQ(step.stage, "clean");
    EXPECT_TRUE(step.overrides.empty());
}

TEST(config, ParseStepInvalidTokenThrows) {
    EXPECT_THROW(std::ignore = baldr::parse_step("install build-type=Release"), nova::exception);
    EXPECT_THROW(std::ignore = baldr::parse_step("install @build-type"), nova::exception);
}

} // namespace
