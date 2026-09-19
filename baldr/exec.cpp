#include <baldr/exec.hpp>

#include <libutl/command.hpp>
#include <libutl/line_reader.hpp>
#include <libutl/rlog.hpp>
#include <libutl/signal.hpp>

#include <libnova/log.hpp>

#include <csignal>

namespace baldr {

auto run_streamed(
        const std::vector<std::string>& args,
        const std::string& working_directory,
        const std::map<std::string, std::string>& env
) -> utl::command::exit_status {
    auto resolved_env = env;
    resolved_env.emplace(utl::rlog::EnvLogMode, "plain");

    auto cmd = utl::command{ args, resolved_env, working_directory };
    cmd.run();
    auto watch = utl::signal_handler::scoped_watch{ SIGINT, cmd.pid() };

    utl::line_reader lines([](std::string& line) { nova::log::info("{}", line); });
    std::string chunk;
    while (chunk = cmd.poll(), not chunk.empty()) {
        lines.feed(chunk);
    }
    lines.feed_eof();

    return cmd.wait();
}

} // namespace baldr
