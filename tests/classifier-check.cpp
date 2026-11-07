/**
 * Part of Baldr tests
 *
 * @brief   Checks a classifier rules file against lines of known level.
 *
 * Usage: `classifier-check <rules file> <lines file>`
 *
 * Each line of the lines file is `<level> <line of output>`; blank lines and
 * lines starting with `#` are skipped. Exits with failure on any mismatch.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-11-06
 * @updated     2026-11-06
 * @reviewed    none
 * SPDX-License-Identifier: BSL-1.0
 */

#include <baldr/classifier.hpp>

#include <libnova/dyn_value.hpp>

#include <fmt/format.h>

#include <spdlog/common.h>

#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

namespace {

[[nodiscard]] auto read_rules(const std::string& path) -> baldr::rules_t {
    return baldr::parse_rules(
        nova::dyn_value::map({ { "classifier", nova::dyn_value::read_config(path).value() } })
    );
}

} // namespace

auto main(int argc, char** argv) -> int {
    if (argc != 3) {
        fmt::println(stderr, "Usage: classifier-check <rules file> <lines file>");
        return EXIT_FAILURE;
    }

    const auto rules = read_rules(argv[1]);

    auto input = std::ifstream{ argv[2] };
    if (not input) {
        fmt::println(stderr, "Can't read `{}`", argv[2]);
        return EXIT_FAILURE;
    }

    std::size_t checked = 0;
    std::size_t failed = 0;

    for (auto text = std::string{ }; std::getline(input, text); ) {
        if (text.empty() or text.starts_with('#')) {
            continue;
        }

        const auto entry = std::string_view{ text };
        const auto split = entry.find(' ');
        const auto expected = spdlog::level::from_str(std::string{ entry.substr(0, split) });
        const auto line = split == std::string_view::npos ? std::string_view{ } : entry.substr(split + 1);
        const auto actual = baldr::classify(rules, line);

        ++checked;

        if (actual != expected) {
            ++failed;
            fmt::println(
                stderr,
                "Expected {}, got {}: {}",
                spdlog::level::to_string_view(expected),
                spdlog::level::to_string_view(actual),
                line
            );
        }
    }

    fmt::println("{} lines checked, {} failed", checked, failed);

    return failed == 0 and checked > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
