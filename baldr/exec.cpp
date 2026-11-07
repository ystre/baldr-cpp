/**
 * Part of Baldr
 *
 * @brief   Shared subprocess-streaming helper.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-08-11
 * @updated     2026-11-06
 * @status      evolving
 * @reviewed    skimmed
 * SPDX-License-Identifier: BSL-1.0
 */

#include <baldr/exec.hpp>

#include <libutl/command.hpp>
#include <libutl/line_reader.hpp>
#include <libutl/log.hpp>
#include <libutl/signal.hpp>

#include <csignal>

namespace baldr {

auto run_streamed(
        const std::vector<std::string>& args,
        const std::string& working_directory,
        const rules_t& rules,
        const std::map<std::string, std::string>& env
) -> utl::command::exit_status {
    auto resolved_env = env;
    resolved_env.merge(utl::log::child_env());

    auto cmd = utl::command{ args, resolved_env, working_directory };
    cmd.run();
    auto watch = utl::signal_handler::scoped_watch{ SIGINT, cmd.pid() };

    utl::line_reader lines([&rules](std::string& line) { log_output(rules, line); });
    std::string chunk;

    while (chunk = cmd.poll(), not chunk.empty()) {
        lines.feed(chunk);
    }

    lines.feed_eof();

    return cmd.wait();
}

} // namespace baldr
