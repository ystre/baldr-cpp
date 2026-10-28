#include <libutl/log.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>

namespace {

using utl::log::format_e;
using utl::log::view_e;

auto resolve_with(std::map<std::string, std::string> env) -> utl::log::settings_t {
    return utl::log::resolve("app", [env = std::move(env)](const std::string& var) -> std::optional<std::string> {
        if (auto found = env.find(var); found != env.end()) {
            return found->second;
        }
        return std::nullopt;
    });
}

} // namespace

TEST(Log, Resolve_Defaults) {
    auto config = resolve_with({});
    EXPECT_EQ(config.terminal_format, format_e::plain);
    EXPECT_EQ(config.terminal_view, view_e::stream);
    EXPECT_EQ(config.file_format, format_e::epoch);
    ASSERT_TRUE(config.file_dir.has_value());
    EXPECT_EQ(config.file_dir->filename(), "app-logs");
}

TEST(Log, Resolve_FormatAndView) {
    auto config = resolve_with({ { "APP_LOG_FORMAT", "timestamped" }, { "APP_LOG_VIEW", "window" } });
    EXPECT_EQ(config.terminal_format, format_e::timestamped);
    EXPECT_EQ(config.terminal_view, view_e::window);
}

TEST(Log, Resolve_EmptyFileDisablesFile) {
    EXPECT_FALSE(resolve_with({ { "APP_LOG_FILE", "" } }).file_dir.has_value());
}

TEST(Log, Resolve_FileDirAndFormat) {
    auto config = resolve_with({ { "APP_LOG_FILE", "/var/tmp/x" }, { "APP_LOG_FILE_FORMAT", "plain" } });
    EXPECT_EQ(config.file_dir, std::filesystem::path{ "/var/tmp/x" });
    EXPECT_EQ(config.file_format, format_e::plain);
}

TEST(Log, Resolve_SizesAndInvalidValuesIgnored) {
    auto config = resolve_with({ { "APP_LOG_WINDOW_SIZE", "5" }, { "APP_LOG_LINE_MAX", "0" }, { "APP_LOG_VIEW", "fancy" } });
    EXPECT_EQ(config.window_size, 5);
    EXPECT_EQ(config.line_max, utl::log::settings_t{}.line_max);
    EXPECT_EQ(config.terminal_view, view_e::stream);
}
