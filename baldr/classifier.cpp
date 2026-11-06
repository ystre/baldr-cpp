/**
 * Part of Baldr
 *
 * @brief   Classifying log levels for the output of children processes.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-10-28
 * @updated     2026-11-06
 * @status      Clean
 */

#include <baldr/classifier.hpp>

#include <libutl/tty.hpp>

#include <libnova/error.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace baldr {

namespace {

/**
 * @brief   The level named at `path`.
 *
 * @throws  if it is missing, not a string, or not a level a line can have.
 *
 * @note    Spdlog's `from_str` gives `off` for an unknown name.
 */
[[nodiscard]] auto parse_level(const nova::dyn_value& doc, const nova::dyn_path& path) -> spdlog::level::level_enum {
    const auto name = doc.lookup<std::string>(path);
    const auto level = spdlog::level::from_str(name);

    if (level == spdlog::level::off) {
        throw nova::exception("`{}`: unknown level `{}`", path, name);
    }

    return level;
}

} // namespace

auto parse_rules(const nova::dyn_value& doc) -> rules_t {
    auto ret = rules_t{ };

    for (const auto& name : doc.lookup<std::vector<std::string>>("classifier.use", { })) {
        const auto set_path = nova::dyn_path{ "classifier", "sets" } / name;
        const auto set = doc.find<nova::dyn_value::seq_t>(set_path);

        if (not set) {
            throw nova::exception("`classifier.use`: unknown set `{}`", name);
        }

        for (std::size_t index = 0; index < set->size(); ++index) {
            const auto rule_path = set_path / index;

            ret.push_back({
                .level = parse_level(doc, rule_path / "level"),
                .markers = doc.lookup<std::vector<std::string>>(rule_path / "contains"),
            });
        }
    }

    return ret;
}

auto classify(const rules_t& rules, std::string_view line) -> spdlog::level::level_enum {
    const auto plain = utl::tty::strip_ansi(line);

    const auto match = std::ranges::find_if(
        rules,
        [&](const rule_t& rule) {
            return std::ranges::any_of(
                rule.markers,
                [&](std::string_view marker) { return plain.contains(marker); }
            );
        }
    );

    return match != rules.end() ? match->level : spdlog::level::info;
}

void log_output(const rules_t& rules, std::string_view line) {
    spdlog::log(classify(rules, line), "{}", line);
}

} // namespace baldr
