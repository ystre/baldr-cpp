#include <baldr/config.hpp>

#include <libnova/error.hpp>
#include <libnova/log.hpp>
#include <libnova/utils.hpp>
#include <libnova/yaml.hpp>

#include <algorithm>
#include <optional>
#include <regex>
#include <sstream>
#include <string>
#include <filesystem>

namespace baldr {

namespace {

/**
 * @brief   Replace every `${KEY}` occurrence in `value` with whatever
 *          `lookup(KEY)` returns, leaving unresolved references untouched.
 */
[[nodiscard]] auto interpolate(const std::string& value, const auto& lookup) -> std::string {
    static const std::regex ref{ R"(\$\{([A-Za-z_][A-Za-z0-9_]*)\})" };

    std::string result;
    auto begin = std::sregex_iterator(value.begin(), value.end(), ref);
    auto end = std::sregex_iterator();
    std::size_t last = 0;

    for (auto it = begin; it != end; ++it) {
        const auto& match = *it;

        result.append(value, last, static_cast<std::size_t>(match.position()) - last);
        if (auto resolved = lookup(match[1].str())) {
            result += *resolved;
        } else {
            throw nova::exception("Variable `{}` is undefined", match[1].str());
        }
        last = static_cast<std::size_t>(match.position() + match.length());
    }
    result.append(value, last, std::string::npos);

    return result;
}

/**
 * @brief   Parse `batch.env` from the configuration.
 *
 * Variables interpolated from the same map.
 */
auto parse_batch_env(const nova::yaml& doc) -> std::map<std::string, std::string> {
    if (not doc.contains("batch.env")) {
        return {};
    }

    auto result = std::map<std::string, std::string>{ };

    auto raw = doc.lookup<std::map>("batch.env");
    for (auto& [key, value_node] : raw) {
        auto resolved = interpolate(value_node.as<std::string>(), [&result](const std::string& ref) -> std::optional<std::string> {
            if (auto it = result.find(ref); it != result.end()) {
                return it->second;
            }
            return std::nullopt;
        });

        result.insert_or_assign(key, std::move(resolved));
    }

    return result;
}

/**
 * @brief   Parse `batch.stages` from the configuration.
 */
auto parse_batch_stages(const nova::yaml& doc) -> std::map<std::string, batch_stage> {
    if (not doc.contains("batch.stages")) {
        return {};
    }

    auto result = std::map<std::string, batch_stage>{ };

    auto raw = doc.lookup<std::map>("batch.stages");
    for (auto& [name, node] : raw) {
        batch_stage stage;
        stage.cmd = node.lookup<std::string>("cmd");
        stage.parameters = node.lookup<std::vector<std::string>>("parameters", {});
        result.insert_or_assign(name, std::move(stage));
    }

    return result;
}

/**
 * @brief   Parse `batch.scenarios` from `doc` on top of `scenarios`.
 */
auto parse_batch_scenarios(const nova::yaml& doc) -> std::map<std::string, std::vector<batch_step>> {
    if (not doc.contains("batch.scenarios")) {
        return {};
    }

    auto result = std::map<std::string, std::vector<batch_step>>{ };

    auto raw = doc.lookup<std::map>("batch.scenarios");
    for (auto& [name, node] : raw) {
        std::vector<batch_step> steps;
        for (const auto& step : node.as<std::vector<std::string>>()) {
            steps.push_back(parse_step(step));
        }
        result.insert_or_assign(name, std::move(steps));
    }

    return result;
}

} // namespace

[[nodiscard]] auto parse_step(const std::string& step) -> batch_step {
    std::istringstream iss{ step };
    batch_step result;
    iss >> result.stage;

    if (result.stage.empty()) {
        throw nova::exception("Invalid batch scenario step: `{}`", step);
    }

    std::string token;
    while (iss >> token) {
        if (not token.starts_with('@')) {
            throw nova::exception("Invalid batch scenario step token `{}` in `{}` (expected `@key=value`)", token, step);
        }

        auto body = token.substr(1);
        auto eq_pos = body.find('=');
        if (eq_pos == std::string::npos) {
            throw nova::exception("Invalid batch scenario step token `{}` in `{}` (expected `@key=value`)", token, step);
        }

        auto key = body.substr(0, eq_pos);
        std::ranges::replace(key, '-', '_');
        result.overrides.insert_or_assign(std::move(key), body.substr(eq_pos + 1));
    }

    return result;
}

[[nodiscard]] auto parse(const nova::yaml& doc, config result) -> config {
    if (doc.contains("debugger")) {
        result.debugger = doc.lookup<std::string>("debugger");
    }

    if (doc.contains("debugger-args")) {
        result.debugger_args = doc.lookup<std::vector<std::string>>("debugger-args");
    }

    if (doc.contains("build-type")) {
        result.build_type = doc.lookup<std::string>("build-type");
    }

    if (doc.contains("cmake.definitions")) {
        auto defines = doc.lookup<std::map<std::string, std::string>>("cmake.definitions");
        for (auto& [key, value] : defines) {
            result.cmake_defines.insert_or_assign(key, std::move(value));
        }
    }

    if (doc.contains("env")) {
        auto env = doc.lookup<std::map<std::string, std::string>>("env");
        for (auto& [key, value] : env) {
            result.env.insert_or_assign(key, std::move(value));
        }
    }

    if (doc.contains("docker.mount-baldr")) {
        result.docker_mount_baldr = doc.lookup<bool>("docker.mount-baldr");
    }

    if (doc.contains("docker.baldr-path")) {
        result.docker_baldr_path = doc.lookup<std::string>("docker.baldr-path");
    }

    result.batch.env = parse_batch_env(doc);
    result.batch.stages = parse_batch_stages(doc);
    result.batch.scenarios = parse_batch_scenarios(doc);

    return result;
}

auto load(std::string_view yaml_content, config result) -> nova::expected<config, nova::error> {
    try {
        auto doc = nova::yaml{ std::string(yaml_content) };
        return parse(doc, std::move(result));
    } catch (const std::exception& ex) {
        return nova::unexpected{ nova::error(ex.what()) };
    }
}

auto load(const std::filesystem::path& path, baldr::config result) -> nova::expected<baldr::config, nova::error> {
    nova::log::debug("Trying to load config `{}`", path.string());
    if (not std::filesystem::exists(path)) {
        return result;
    }

    nova::log::debug("Config `{}` loaded", path.string());

    try {
        auto doc = nova::yaml{ path };
        return parse(doc, std::move(result));
    } catch (const std::exception& ex) {
        return nova::unexpected{ nova::error(ex.what()) };
    }
}

auto load(const std::filesystem::path& project_dir) -> nova::expected<config, nova::error> {
    const auto home = std::filesystem::path{ nova::getenv("HOME").value() };
    auto config_default = config{ };

    auto global = load(home / ".baldr.yaml", config_default);
    if (global) {
        config_default = *global;
    }

    return load(project_dir / ".baldr.yaml", config_default);
}

} // namespace baldr
