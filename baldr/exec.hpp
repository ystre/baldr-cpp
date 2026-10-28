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
 * @brief   Run `args` inside `working_directory`, logging its combined
 *          stdout/stderr output line by line at the `classify()`d level.
 *
 * Registers the spawned child with `signal_handler::watch()` for `SIGINT`
 * for the duration of the run (see `baldr/signal.hpp`), so repeated Ctrl-C
 * still takes it down even if it's ignoring/outliving the signal itself.
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
    const std::map<std::string, std::string>& env = {}
) -> utl::command::exit_status;

} // namespace baldr
