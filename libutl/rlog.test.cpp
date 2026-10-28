#include <libutl/rlog.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>

namespace {

using utl::rlog::format;
using utl::rlog::view;

auto resolve_with(std::map<std::string, std::string> env) -> utl::rlog::settings {
    return utl::rlog::resolve("app", [env = std::move(env)](const std::string& var) -> std::optional<std::string> {
        if (auto it = env.find(var); it != env.end()) {
            return it->second;
        }
        return std::nullopt;
    });
}

} // namespace

TEST(Rlog, EnvName_UppercasesAndSanitizes) {
    EXPECT_EQ(utl::rlog::env_name("baldr", "VIEW"), "BALDR_LOG_VIEW");
    EXPECT_EQ(utl::rlog::env_name("my-app", "FILE"), "MY_APP_LOG_FILE");
}

TEST(Rlog, Resolve_Defaults) {
    auto s = resolve_with({});
    EXPECT_EQ(s.terminal_format, format::plain);
    EXPECT_EQ(s.terminal_view, view::stream);
    EXPECT_EQ(s.file_format, format::epoch);
    ASSERT_TRUE(s.file_dir.has_value());
    EXPECT_EQ(s.file_dir->filename(), "app-logs");
}

TEST(Rlog, Resolve_ClaudeCodeSelectsDigest) {
    EXPECT_EQ(resolve_with({ { "CLAUDECODE", "1" } }).terminal_view, view::digest);
}

TEST(Rlog, Resolve_ExplicitViewBeatsClaudeCode) {
    EXPECT_EQ(resolve_with({ { "CLAUDECODE", "1" }, { "APP_LOG_VIEW", "stream" } }).terminal_view, view::stream);
}

TEST(Rlog, Resolve_LegacyModeSetsFormatAndView) {
    auto s = resolve_with({ { "SPDLOG_MODE", "rlog" } });
    EXPECT_EQ(s.terminal_format, format::timestamped);
    EXPECT_EQ(s.terminal_view, view::window);
}

TEST(Rlog, Resolve_NewVariablesBeatLegacyMode) {
    auto s = resolve_with({ { "SPDLOG_MODE", "rlog" }, { "APP_LOG_FORMAT", "epoch" } });
    EXPECT_EQ(s.terminal_format, format::epoch);
    EXPECT_EQ(s.terminal_view, view::window);
}

TEST(Rlog, Resolve_EmptyFileDisablesFile) {
    EXPECT_FALSE(resolve_with({ { "APP_LOG_FILE", "" } }).file_dir.has_value());
}

TEST(Rlog, Resolve_FileDirAndFormat) {
    auto s = resolve_with({ { "APP_LOG_FILE", "/var/tmp/x" }, { "APP_LOG_FILE_FORMAT", "plain" } });
    EXPECT_EQ(s.file_dir, std::filesystem::path{ "/var/tmp/x" });
    EXPECT_EQ(s.file_format, format::plain);
}

TEST(Rlog, Resolve_SizesAndInvalidValuesIgnored) {
    auto s = resolve_with({ { "APP_LOG_WINDOW_SIZE", "5" }, { "APP_LOG_LINE_MAX", "0" }, { "APP_LOG_VIEW", "fancy" } });
    EXPECT_EQ(s.window_size, 5);
    EXPECT_EQ(s.line_max, utl::rlog::settings{}.line_max);
    EXPECT_EQ(s.terminal_view, view::stream);
}

TEST(Rlog, Resolve_LegacyWindowSize) {
    EXPECT_EQ(resolve_with({ { "RLOG_WINDOW_SIZE", "3" } }).window_size, 3);
}
