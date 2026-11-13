/**
 * Part of Baldr
 *
 * @brief   The targets of a CMake build directory, via the CMake File API.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-11-13
 * @updated     2026-11-13
 * @status      evolving
 * @reviewed    skimmed
 * SPDX-License-Identifier: BSL-1.0
 */

#include <baldr/codemodel.hpp>

#include <libnova/error.hpp>
#include <libnova/io.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr auto QueryFile = ".cmake/api/v1/query/codemodel-v2";
constexpr auto ReplyDir = ".cmake/api/v1/reply";

constexpr auto DashboardPrefixes = std::array<std::string_view, 3>{ "Nightly", "Continuous", "Experimental" };

[[nodiscard]] auto parse_json(std::string_view text, std::string_view file) -> nlohmann::json {
    auto ret = nlohmann::json::parse(text, nullptr, /*allow_exceptions=*/false);
    if (ret.is_discarded()) {
        throw nova::exception("Malformed CMake File API reply `{}`", file);
    }

    return ret;
}

/**
 * @brief   The configuration named `name` in a code model, or its first one.
 */
[[nodiscard]] auto pick_configuration(const nlohmann::json& codemodel, std::string_view name) -> const nlohmann::json& {
    const auto& configurations = codemodel.at("configurations");
    if (configurations.empty()) {
        throw nova::exception("CMake code model has no configuration");
    }

    const auto found = std::ranges::find_if(configurations, [name](const nlohmann::json& configuration) {
        return configuration.value("name", "") == name;
    });

    return found != configurations.end() ? *found : configurations.front();
}

[[nodiscard]] auto to_lower(std::string text) -> std::string {
    std::ranges::transform(text, text.begin(), [](unsigned char letter) { return std::tolower(letter); });
    return text;
}

[[nodiscard]] auto is_listed(const baldr::target_t& target) -> bool {
    if (target.type == "interface_library") {
        return false;
    }

    return not std::ranges::any_of(DashboardPrefixes, [&target](std::string_view prefix) {
        return target.type == "utility" and target.name.starts_with(prefix);
    });
}

} // namespace

namespace baldr {

auto request_codemodel(const fs::path& build_dir) -> bool {
    const auto query = build_dir / QueryFile;
    if (fs::exists(query)) {
        return false;
    }

    fs::create_directories(query.parent_path());
    std::ofstream{ query };

    return true;
}

auto latest_index(const std::vector<std::string>& files) -> std::optional<std::string> {
    auto ret = std::optional<std::string>{ };

    for (const auto& file : files) {
        if (file.starts_with("index-") and file.ends_with(".json") and (not ret or file > *ret)) {
            ret = file;
        }
    }

    return ret;
}

auto parse_codemodel(std::string_view index, const reply_reader& read_reply, std::string_view configuration)
    -> std::optional<std::vector<target_t>>
try {
    const auto index_json = parse_json(index, "index");
    const auto reply = index_json.at("reply").find("codemodel-v2");
    if (reply == index_json.at("reply").end()) {
        return std::nullopt;
    }

    if (reply->contains("error")) {
        throw nova::exception("CMake File API: {}", reply->at("error").get<std::string>());
    }

    const auto read_json = [&read_reply](const nlohmann::json& file) {
        const auto name = file.get<std::string>();
        return parse_json(read_reply(name), name);
    };

    const auto codemodel = read_json(reply->at("jsonFile"));

    auto ret = std::vector<target_t>{ };

    for (const auto& entry : pick_configuration(codemodel, configuration).at("targets")) {
        auto target = target_t{
            .name = entry.at("name").get<std::string>(),
            .type = to_lower(read_json(entry.at("jsonFile")).at("type").get<std::string>()),
        };

        if (is_listed(target)) {
            ret.push_back(std::move(target));
        }
    }

    std::ranges::sort(ret, { }, &target_t::name);

    return ret;
} catch (const nlohmann::json::exception& ex) {
    throw nova::exception("Malformed CMake File API reply: {}", ex.what());
}

auto read_codemodel(const fs::path& build_dir, std::string_view configuration)
    -> std::optional<std::vector<target_t>>
{
    const auto reply_dir = build_dir / ReplyDir;

    auto files = std::vector<std::string>{ };
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(reply_dir, ec)) {
        files.push_back(entry.path().filename().string());
    }

    const auto index = latest_index(files);
    if (not index) {
        return std::nullopt;
    }

    const auto read_reply = [&reply_dir](const std::string& file) -> std::string {
        auto text = nova::read_file((reply_dir / file).string());
        if (not text) {
            throw nova::exception("Cannot read CMake File API reply `{}`", (reply_dir / file).string());
        }
        return std::move(*text);
    };

    return parse_codemodel(read_reply(*index), read_reply, configuration);
}

} // namespace baldr
