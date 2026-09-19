/**
 * Part of Baldr
 *
 * Shared subprocess-streaming helper, used by both `builder` (build/run) and
 * `batch_runner`.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-08-11
 */

#pragma once

#include <libutl/command.hpp>

#include <map>
#include <string>
#include <vector>

namespace baldr {

/**
 * @brief   Run `args` inside `working_directory`, streaming its combined
 *          stdout/stderr output via `nova::log::info`.
 *
 * Registers the spawned child with `signal_handler::watch()` for `SIGINT`
 * for the duration of the run (see `baldr/signal.hpp`), so repeated Ctrl-C
 * still takes it down even if it's ignoring/outliving the signal itself.
 *
 * Unless `env` already sets it, `SPDLOG_MODE=plain` is added to the child's
 * environment, so a spawned process that's itself an `utl::rlog` user (e.g.
 * a nested `baldr` invocation) doesn't attach its own timestamp/name/level
 * prefix on top of the one this function's own re-logging already adds per
 * line (see `utl::rlog::mode::plain` in `libutl/rlog.hpp`).
 *
 * @return  The command's detailed outcome (see `baldr::command::exit_status`),
 *          so callers can tell a `SIGINT` interruption apart from an actual
 *          failure.
 */
[[nodiscard]] auto run_streamed(
    const std::vector<std::string>& args,
    const std::string& working_directory,
    const std::map<std::string, std::string>& env = {}
) -> utl::command::exit_status;

} // namespace baldr
