/**
 * Part of Utility Library
 *
 * Example application demonstrating `utl::rlog`'s scrolling-window
 * rendering: a fixed-size rolling window of the most recent log lines,
 * collapsed into a final success/failure message once the tracked work
 * completes.
 *
 * @author      Gábor Krisztián Girhiny
 * @date        2026-07-12
 */

#include <libutl/rlog.hpp>

#include <libnova/log.hpp>

#include <array>
#include <chrono>
#include <thread>

int main() {
    utl::rlog::init("rlog-example");

    static constexpr auto steps = std::to_array<const char*>({
        "Resolving dependencies...",
        "Fetching sources...",
        "Configuring build...",
        "Compiling module 'tty'...",
        "Compiling module 'rlog'...",
        "Compiling module 'baldr'...",
        "Linking executable...",
        "Running test suite...",
    });

    for (const char* step: steps) {
        nova::log::info("{}", step);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    utl::rlog::failure("Task failed.");

    for (const char* step: steps) {
        nova::log::info("{}", step);
        std::this_thread::sleep_for(std::chrono::milliseconds(400));
    }

    utl::rlog::success("Task finished successfully.");

    return 0;
}
