/**
 * Part of Baldr
 *
 * @brief   Shared subprocess-streaming helper.
 *
 * Used by both `builder` (build/run) and `batch_runner`.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-08-11
 * @updated     2026-11-13
 * @status      evolving
 * @reviewed    skimmed
 * SPDX-License-Identifier: BSL-1.0
 */

#pragma once

#include <baldr/classifier.hpp>

#include <libutl/command.hpp>

#include <map>
#include <string>
#include <vector>

namespace baldr {

/**
 * @brief   Run `args` inside `working_directory`, logging its combined
 *          stdout/stderr output line by line at the `classify()`d level.
 *
 * `utl::log::child_env()` is added to the child's environment where `env`
 * doesn't set the same variables, so a nested `baldr` streams plain lines
 * for this process to re-log, and writes no log file of its own.
 *
 * @return  The command's detailed outcome (see `baldr::command::exit_status`),
 *          so callers can tell a `SIGINT` interruption apart from an actual
 *          failure.
 */
[[nodiscard]] auto run_streamed(
    const std::vector<std::string>& args,
    const std::string& working_directory,
    const rules_t& rules,
    const std::map<std::string, std::string>& env = {}
) -> utl::command::exit_status;

} // namespace baldr
