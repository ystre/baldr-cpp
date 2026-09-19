/**
 * Part of Utility Library
 *
 * Example application demonstrating `utl::command`
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 *
 * Spawning a child process and streaming its combined stdout/stderr output
 * line-by-line via `utl::line_reader`.
 */

#include <libutl/command.hpp>
#include <libutl/line_reader.hpp>
#include <libnova/log.hpp>

#include <cstdlib>
#include <string>
#include <vector>

int main() {
    nova::log::init("command-example");

    std::vector<std::string> args = {
        "bash", "-c",
        "for i in 1 2 3 4 5; do echo \"Step $i/5...\"; sleep 0.3; done"
    };

    auto cmd = utl::command{ args };
    cmd.run();

    auto logger = [](std::string& line) { nova::log::info("{}", line); };
    auto lines = utl::line_reader{ logger };

    std::string chunk;
    while (chunk = cmd.poll(), not chunk.empty()) {
        lines.feed(chunk);
    }
    lines.feed_eof();

    auto status = cmd.wait();
    if (status.success()) {
        nova::log::info("Command finished successfully.");
    } else {
        nova::log::error("{}", status.describe());
    }

    return status.code();
}
