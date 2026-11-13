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
 *
 * Baldr places a shared, client-less `codemodel-v2` query in every build
 * directory it configures; CMake answers it on each (re-)generation.
 */

#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baldr {

/**
 * @brief   A buildable target.
 */
struct target_t {
    std::string name;
    std::string type;       ///< CMake's target type in lower case, e.g. `executable`, `static_library`, `utility`.

    auto operator==(const target_t&) const -> bool = default;
};

/**
 * @brief   Ask CMake for the code model of `build_dir` from its next
 *          (re-)generation on.
 *
 * @return  Whether the request is new, i.e. `build_dir` has no reply to it
 *          until CMake generates again.
 */
auto request_codemodel(const std::filesystem::path& build_dir) -> bool;

/**
 * @brief   Reads a file of the reply directory by name, e.g.
 *          `codemodel-v2-<hash>.json`.
 *
 * @throws  nova::exception if the file cannot be read.
 */
using reply_reader = std::function<std::string(const std::string& file)>;

/**
 * @brief   The newest `index-*.json` of the reply directory's `files`; the
 *          names embed a timestamp, so the greatest one is the latest.
 */
[[nodiscard]] auto latest_index(const std::vector<std::string>& files) -> std::optional<std::string>;

/**
 * @brief   The buildable targets of `configuration` in the code model reply
 *          that `index`, the text of an index file, points to, sorted by
 *          name.
 *
 * Interface libraries, which build nothing, and the CTest dashboard targets
 * (`Nightly*`, `Continuous*`, `Experimental*`) are left out. A
 * single-configuration build has one configuration only, which is used if
 * none is named `configuration`.
 *
 * @param   read_reply  Reads the code model and target files `index` refers to.
 *
 * @return  `std::nullopt` if the index holds no reply to the request.
 *
 * @throws  nova::exception if the reply is malformed or reports an error.
 */
[[nodiscard]] auto parse_codemodel(std::string_view index, const reply_reader& read_reply, std::string_view configuration)
    -> std::optional<std::vector<target_t>>;

/**
 * @brief   `parse_codemodel()` of `build_dir`'s latest reply.
 *
 * @return  `std::nullopt` if there is no reply (yet).
 */
[[nodiscard]] auto read_codemodel(const std::filesystem::path& build_dir, std::string_view configuration)
    -> std::optional<std::vector<target_t>>;

} // namespace baldr
