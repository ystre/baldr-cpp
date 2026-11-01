/**
 * Part of Utility Library
 *
 * @brief   Environment variable helpers.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-10-31
 * @updated     2026-10-31
 */

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace utl {

/**
 * @brief   Name of the environment variable `<PREFIX>_<KEY>`.
 *
 * Both parts are upper-cased and every non-alphanumeric character becomes
 * `_`, so any identifier (e.g. a program or logger name) maps to a valid,
 * conventional variable name: `env_name("my-app", "log_file")` is
 * `MY_APP_LOG_FILE`.
 */
[[nodiscard]] inline auto env_name(std::string_view prefix, std::string_view key) -> std::string {
    std::string result;
    result.reserve(prefix.size() + 1 + key.size());

    const auto append = [&result](std::string_view part) {
        for (const char character : part) {
            const auto byte = static_cast<unsigned char>(character);
            result += std::isalnum(byte) != 0 ? static_cast<char>(std::toupper(byte)) : '_';
        }
    };

    append(prefix);
    result += '_';
    append(key);

    return result;
}

} // namespace utl
