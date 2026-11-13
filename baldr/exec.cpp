/**
 * Part of Baldr
 *
 * @brief   Shared subprocess-streaming helper.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-08-11
 * @updated     2026-11-13
 * @status      evolving
 * @reviewed    skimmed
 * SPDX-License-Identifier: BSL-1.0
 */

#include <baldr/exec.hpp>

#include <libutl/command.hpp>
#include <libutl/line_reader.hpp>
#include <libutl/log.hpp>
#include <libutl/signal.hpp>

#include <libnova/log.hpp>

#include <csignal>

namespace baldr {

namespace {

/**
 * @brief   Log the `SIGINT` deliveries.
 *
 * @return  The number of deliveries logged so far.
 */
auto log_interrupts(int seen, const std::string& name) -> int {
    constexpr auto kill_after = utl::signal_handler::ForceKillAfter;

    for (auto count = utl::signal_handler::triggered(SIGINT); seen < count; ) {
        ++seen;
        if (seen < kill_after) {
            auto left = kill_after - seen;
            nova::log::warn("Interrupt received: stopping `{}`, {} more interrupt(s) to force-kill.", name, left);
        } else if (seen == kill_after) {
            nova::log::warn("Interrupt received: force-killing `{}`.", name);
        }
    }

    return seen;
}

} // namespace

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
    auto watch = utl::signal_handler::scoped_watch{ SIGINT, cmd.pid(), utl::forward_e::group };
    auto interrupts = utl::signal_handler::triggered(SIGINT);

    utl::line_reader lines([&rules](std::string& line) { log_output(rules, line); });
    std::string chunk;

    while (chunk = cmd.poll(), not chunk.empty()) {
        interrupts = log_interrupts(interrupts, args.front());
        lines.feed(chunk);
    }

    lines.feed_eof();
    log_interrupts(interrupts, args.front());

    return cmd.wait();
}

} // namespace baldr
