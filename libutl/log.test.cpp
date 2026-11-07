/**
 * Part of Utility Library
 *
 * @brief   Tests of `log.hpp`.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-10-28
 * @updated     2026-11-06
 * @reviewed    skimmed
 * SPDX-License-Identifier: BSL-1.0
 */

#include <libutl/log.hpp>

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include <map>
#include <memory>
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

/**
 * @brief   What a digest of at most `level_max` lines per level prints for the
 *          records logged by `emit`, then its summary.
 */
template <typename Callable>
auto digest_of(std::size_t level_max, Callable emit) -> std::string {
    auto sink = std::make_shared<utl::log::digest_sink>(160, level_max);
    auto logger = spdlog::logger("digest-test", sink);
    logger.set_pattern("%v");

    testing::internal::CaptureStderr();
    emit(logger);
    sink->failure("Failed.");
    return testing::internal::GetCapturedStderr();
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

TEST(Log, Resolve_DigestLines) {
    EXPECT_EQ(resolve_with({ { "APP_LOG_DIGEST_LINES", "7" } }).digest_lines, 7);
    EXPECT_EQ(resolve_with({ { "APP_LOG_DIGEST_LINES", "0" } }).digest_lines, utl::log::settings_t{}.digest_lines);
}

TEST(Log, Digest_UnderTheLimitShowsEverything) {
    const auto output = digest_of(2, [](spdlog::logger& logger) {
        logger.error("first");
        logger.error("second");
    });

    EXPECT_EQ(output, "first\nsecond\nFailed. 2 errors\n");
}

TEST(Log, Digest_OverTheLimitKeepsTheFirstAndCountsTheRest) {
    const auto output = digest_of(1, [](spdlog::logger& logger) {
        logger.error("first");
        logger.error("second");
        logger.error("third");
    });

    EXPECT_EQ(output, "first\nFailed. 3 errors, 2 not shown\n");
}

TEST(Log, Digest_WarningsDoNotCrowdOutErrors) {
    const auto output = digest_of(1, [](spdlog::logger& logger) {
        logger.warn("noise");
        logger.warn("more noise");
        logger.error("the failure");
    });

    EXPECT_EQ(output, "noise\nthe failure\nFailed. 1 error, 2 warnings, 1 not shown\n");
}

TEST(Log, Digest_RepeatsAreNotHidden) {
    const auto output = digest_of(1, [](spdlog::logger& logger) {
        logger.error("first");
        logger.error("first");
    });

    EXPECT_EQ(output, "first\nFailed. 1 error, 1 repeated\n");
}

TEST(Log, Resolve_SizesAndInvalidValuesIgnored) {
    auto config = resolve_with({ { "APP_LOG_WINDOW_SIZE", "5" }, { "APP_LOG_LINE_WIDTH", "0" }, { "APP_LOG_VIEW", "fancy" } });
    EXPECT_EQ(config.window_size, 5);
    EXPECT_EQ(config.line_width, utl::log::settings_t{}.line_width);
    EXPECT_EQ(config.terminal_view, view_e::stream);
}
