/**
 * Part of Baldr
 *
 * @brief   Classifying log levels for the output of children processes.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-10-28
 * @updated     2026-11-06
 * @status      evolving
 * @reviewed    read
 * SPDX-License-Identifier: BSL-1.0
 *
 * Intended to be used only for children processes.
 */

#pragma once

#include <libnova/dyn_value.hpp>

#include <spdlog/common.h>

#include <string>
#include <string_view>
#include <vector>

namespace baldr {

/**
 * @brief   The level of the lines having any of `markers`.
 */
struct rule_t {
    spdlog::level::level_enum level = spdlog::level::info;
    std::vector<std::string> markers;
};

/**
 * @brief   The rules in effect, in matching order.
 */
using rules_t = std::vector<rule_t>;

/**
 * @brief   The rules of the sets named by `classifier.use` in `doc`, read from
 *          `classifier.sets`.
 *
 * @throws  if a value has the wrong type, or a level or a set is unknown.
 */
[[nodiscard]] auto parse_rules(const nova::dyn_value& doc) -> rules_t;

/**
 * @brief   The level of the first rule matching `line`; `info` if none does.
 *
 * ANSI escapes in `line` are ignored.
 */
[[nodiscard]] auto classify(const rules_t& rules, std::string_view line) -> spdlog::level::level_enum;

/**
 * @brief   Log a line at its `classify()`d level.
 */
void log_output(const rules_t& rules, std::string_view line);

} // namespace baldr
