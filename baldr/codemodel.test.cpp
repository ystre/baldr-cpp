/**
 * Part of Baldr
 *
 * @brief   Tests of `codemodel.hpp`.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-11-13
 * @updated     2026-11-13
 * @reviewed    none
 * SPDX-License-Identifier: BSL-1.0
 */

#include <baldr/codemodel.hpp>

#include <libnova/error.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <map>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

namespace {

using ::testing::ElementsAre;

constexpr auto Index = R"({ "reply": { "codemodel-v2": { "jsonFile": "codemodel.json" } } })";

/**
 * @brief   A reply directory in memory, file name to content.
 */
using reply_files = std::map<std::string, std::string>;

[[nodiscard]] auto reader(const reply_files& files) -> baldr::reply_reader {
    return [&files](const std::string& file) -> std::string {
        if (auto found = files.find(file); found != files.end()) {
            return found->second;
        }
        throw nova::exception("No reply file `{}`", file);
    };
}

[[nodiscard]] auto target_file(std::string_view type) -> std::string {
    return std::string{ R"({ "type": ")" } + std::string{ type } + R"(" })";
}

/**
 * @brief   `codemodel.json` with one configuration named `Debug`, holding
 *          `targets` (a JSON array).
 */
[[nodiscard]] auto debug_codemodel(std::string_view targets) -> std::string {
    return std::string{ R"({ "configurations": [ { "name": "Debug", "targets": )" } + std::string{ targets } + "} ] }";
}

TEST(Codemodel, LatestIndex) {
    EXPECT_EQ(baldr::latest_index({ }), std::nullopt);
    EXPECT_EQ(baldr::latest_index({ "codemodel-v2-1.json", "cache-v2-2.json" }), std::nullopt);
    EXPECT_EQ(
        baldr::latest_index({ "index-2026-11-13T09-00-00-0000.json", "index-2026-11-13T10-00-00-0000.json", "index-x.txt" }),
        "index-2026-11-13T10-00-00-0000.json"
    );
}

TEST(Codemodel, NoReplyToTheRequest) {
    EXPECT_EQ(baldr::parse_codemodel(R"({ "reply": { } })", reader({ }), "Debug"), std::nullopt);
}

TEST(Codemodel, TargetsSortedWithTheirType) {
    const auto files = reply_files{
        { "codemodel.json", debug_codemodel(R"([
            { "name": "hello", "jsonFile": "hello.json" },
            { "name": "core", "jsonFile": "core.json" }
        ])") },
        { "hello.json", target_file("EXECUTABLE") },
        { "core.json", target_file("STATIC_LIBRARY") },
    };

    const auto expected = std::vector<baldr::target_t>{
        { .name = "core", .type = "static_library" },
        { .name = "hello", .type = "executable" },
    };
    EXPECT_EQ(baldr::parse_codemodel(Index, reader(files), "Debug"), expected);
}

TEST(Codemodel, SkipsUnbuildableAndDashboardTargets) {
    const auto files = reply_files{
        { "codemodel.json", debug_codemodel(R"([
            { "name": "headers", "jsonFile": "headers.json" },
            { "name": "Nightly", "jsonFile": "nightly.json" },
            { "name": "ExperimentalTest", "jsonFile": "experimental.json" },
            { "name": "NightlyBuild", "jsonFile": "nightly-build.json" },
            { "name": "docs", "jsonFile": "docs.json" }
        ])") },
        { "headers.json", target_file("INTERFACE_LIBRARY") },
        { "nightly.json", target_file("UTILITY") },
        { "experimental.json", target_file("UTILITY") },
        { "nightly-build.json", target_file("EXECUTABLE") },
        { "docs.json", target_file("UTILITY") },
    };

    const auto targets = baldr::parse_codemodel(Index, reader(files), "Debug");
    ASSERT_TRUE(targets.has_value());
    EXPECT_THAT(*targets, ElementsAre(
        baldr::target_t{ .name = "NightlyBuild", .type = "executable" },
        baldr::target_t{ .name = "docs", .type = "utility" }
    ));
}

TEST(Codemodel, PicksTheNamedConfiguration) {
    const auto files = reply_files{
        { "codemodel.json", R"({ "configurations": [
            { "name": "Debug", "targets": [ { "name": "debug_only", "jsonFile": "debug.json" } ] },
            { "name": "Release", "targets": [ { "name": "release_only", "jsonFile": "release.json" } ] }
        ] })" },
        { "debug.json", target_file("EXECUTABLE") },
        { "release.json", target_file("EXECUTABLE") },
    };

    const auto release = baldr::parse_codemodel(Index, reader(files), "Release");
    ASSERT_TRUE(release.has_value());
    EXPECT_THAT(*release, ElementsAre(baldr::target_t{ .name = "release_only", .type = "executable" }));

    const auto fallback = baldr::parse_codemodel(Index, reader(files), "MinSizeRel");
    ASSERT_TRUE(fallback.has_value());
    EXPECT_THAT(*fallback, ElementsAre(baldr::target_t{ .name = "debug_only", .type = "executable" }));
}

TEST(Codemodel, ReplyErrorThrows) {
    EXPECT_THROW(
        std::ignore = baldr::parse_codemodel(R"({ "reply": { "codemodel-v2": { "error": "unknown kind" } } })", reader({ }), "Debug"),
        nova::exception
    );
}

TEST(Codemodel, MalformedReplyThrows) {
    EXPECT_THROW(std::ignore = baldr::parse_codemodel("not json", reader({ }), "Debug"), nova::exception);
    EXPECT_THROW(std::ignore = baldr::parse_codemodel(R"({ "no_reply": { } })", reader({ }), "Debug"), nova::exception);

    const auto no_configurations = reply_files{ { "codemodel.json", R"({ "configurations": [] })" } };
    EXPECT_THROW(std::ignore = baldr::parse_codemodel(Index, reader(no_configurations), "Debug"), nova::exception);

    const auto broken_target = reply_files{
        { "codemodel.json", debug_codemodel(R"([ { "name": "hello", "jsonFile": "hello.json" } ])") },
        { "hello.json", "{ \"type\": " },
    };
    EXPECT_THROW(std::ignore = baldr::parse_codemodel(Index, reader(broken_target), "Debug"), nova::exception);
}

TEST(Codemodel, MissingReplyFileThrows) {
    EXPECT_THROW(std::ignore = baldr::parse_codemodel(Index, reader({ }), "Debug"), nova::exception);
}

} // namespace
