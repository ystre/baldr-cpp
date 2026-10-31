/**
 * Part of Baldr
 *
 * @brief   Configuration loading.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-07-19
 * @updated     2026-11-06
 * @status      Clean
 */

#include <baldr/config.hpp>

#include <libnova/dyn_value.hpp>
#include <libnova/error.hpp>
#include <libnova/log.hpp>
#include <libnova/utils.hpp>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <array>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

namespace baldr {

namespace {

/**
 * @brief   The map at `path` with each scalar value as its text.
 *
 * @throws  if it is not a map, or a value is a map or a sequence.
 */
[[nodiscard]] auto text_map(const nova::dyn_value& doc, const nova::dyn_path& path)
        -> std::map<std::string, std::string>
{
    auto ret = std::map<std::string, std::string>{ };

    for (const auto& [key, item] : doc.lookup<nova::dyn_value::map_t>(path, { })) {
        const auto& data = item.data();

        if (std::holds_alternative<nova::dyn_value::map_t>(data) or std::holds_alternative<nova::dyn_value::seq_t>(data)) {
            throw nova::exception("`{}`: expected a scalar, got {}", path / key, item.type_name());
        }

        ret.emplace(key, fmt::format("{}", item));
    }

    return ret;
}

} // namespace

auto parse(const nova::dyn_value& doc) -> config {
    auto ret = config{ };

    try {
        ret.debugger = doc.lookup("debugger", ret.debugger);
        ret.debugger_args = doc.lookup("debugger-args", ret.debugger_args);
        ret.build_type = doc.lookup("build-type", ret.build_type);
        ret.cmake_defines = text_map(doc, "cmake.definitions");
        ret.env = text_map(doc, "env");
        ret.docker_mount_baldr = doc.lookup("docker.mount-baldr", ret.docker_mount_baldr);
        ret.docker_baldr_path = doc.find<std::string>("docker.baldr-path");
    } catch (const nova::exception& ex) {
        throw nova::exception("Config {}", ex.what());
    }

    return ret;
}

auto find_config(const std::filesystem::path& dir) -> std::optional<std::filesystem::path> {
    static constexpr auto extensions = std::to_array<std::string_view>({ ".yaml", ".yml", ".json", ".toml" });

    auto found = std::vector<std::filesystem::path>{ };
    for (const auto extension : extensions) {
        auto path = dir / fmt::format(".baldr{}", extension);
        auto ec = std::error_code{ };

        const auto present = std::filesystem::exists(path, ec);

        if (ec) {
            throw nova::exception("Cannot access `{}`: {}", path.string(), ec.message());
        }

        if (present) {
            found.push_back(std::move(path));
        }
    }

    if (found.size() > 1) {
        auto names = std::vector<std::string>{ };

        for (const auto& path : found) {
            names.push_back(path.filename().string());
        }

        throw nova::exception("Multiple config files in `{}`: {}; keep only one", dir.string(), fmt::join(names, ", "));
    }

    if (found.empty()) {
        return std::nullopt;
    }

    return found.front();
}

auto read(const std::filesystem::path& dir) -> nova::dyn_value {
    const auto file = find_config(dir);
    if (not file) {
        nova::log::debug("No config in `{}`", dir.string());
        return nova::dyn_value::map({ });
    }

    auto doc = nova::dyn_value::read_config(*file);
    if (not doc) {
        throw nova::exception("Failed to read config: {}", doc.error());
    }

    nova::log::debug("Config `{}` loaded", file->string());
    return std::move(*doc);
}

auto load(const std::filesystem::path& project_dir) -> config {
    const auto home = std::filesystem::path{ nova::getenv("HOME").value() };
    auto global = read(home);
    return parse(
        nova::dyn_value::merge(
            std::move(global),
            read(project_dir)
        )
    );
}

} // namespace baldr
