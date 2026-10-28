/**
 * Part of Baldr
 *
 * Log levels for the output of build tools.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Opus 5.5
 * @date        2026-10-28
 */

#pragma once

#include <spdlog/common.h>

#include <string_view>

namespace baldr {

/**
 * @brief   The level a line of build tool output deserves: `err` for
 *          compiler/linker/CMake/make/ninja/test failures, `warn` for
 *          warnings, `info` for the rest.
 *
 * ANSI escapes in `line` are ignored.
 */
[[nodiscard]] auto classify(std::string_view line) -> spdlog::level::level_enum;

/**
 * @brief   Log a line of subprocess output at its `classify()`d level.
 */
void log_output(std::string_view line);

} // namespace baldr
