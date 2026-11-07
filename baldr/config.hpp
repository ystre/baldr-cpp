/**
 * Part of Baldr
 *
 * @brief   Configuration loading.
 *
 * Config files are read into `nova::dyn_value` trees and merged before
 * parsing: maps merge key by key, a null deletes the key, anything else in a
 * later file replaces the earlier value.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-07-19
 * @updated     2026-11-06
 * @status      evolving
 * @reviewed    read
 * SPDX-License-Identifier: BSL-1.0
 */

#pragma once

#include <baldr/classifier.hpp>

#include <libnova/dyn_value.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baldr {

/**
 * @brief   Per-project settings, merged from the config file in HOME (global)
 *          and from the project-local.
 */
struct config {
    std::string debugger = "gdb";
    std::vector<std::string> debugger_args = { "--args" };
    std::string build_type = "Debug";
    std::map<std::string, std::string> cmake_defines;
    std::map<std::string, std::string> env;

    bool docker_mount_baldr = true;
    std::optional<std::string> docker_baldr_path;

    rules_t classifier_rules;
};

/**
 * @brief   The directory of the files shipped with the Baldr binary at `binary`.
 *
 * `<prefix>/share/baldr` beside the directory of the binary.
 */
[[nodiscard]] auto share_dir(const std::filesystem::path& binary) -> std::filesystem::path;

/**
 * @brief   Parse a `config` out of `doc`.
 *
 * Keeps the defaults for anything missing or null in `doc`.
 *
 * The values of `env` and `cmake.definitions` take any scalar, as its text;
 * everything else must have the exact type.
 *
 * @throws  if a value has the wrong type.
 */
[[nodiscard]] auto parse(const nova::dyn_value& doc) -> config;

/**
 * @brief   The config file `stem` in `dir`, with the extension `.yaml`, `.yml`,
 *          `.json` or `.toml`.
 *
 * @returns empty if there is none.
 *
 * @throws  if there is more than one, or `dir` can't be searched.
 */
[[nodiscard]] auto find_config(const std::filesystem::path& dir, std::string_view stem) -> std::optional<std::filesystem::path>;

/**
 * @brief   Read the `.baldr` config file in `dir`, see `find_config`.
 *
 * @returns an empty map if there is none, or it is empty.
 *
 * @throws  if there is more than one, or it can't be read, is malformed or is
 *          not a map.
 */
[[nodiscard]] auto read(const std::filesystem::path& dir) -> nova::dyn_value;

/**
 * @brief   Load the config for `project_dir`.
 *
 * Order of config merging:
 * - The files shipped in `share_dir`
 * - global config (in `$HOME`)
 * - project-local one (in `project_dir`)
 *
 * Each can be in any format, independently of the others.
 *
 * Missing the shipped classifier rules results in a warning.
 *
 * @throws  if a config file fails to load.
 */
[[nodiscard]] auto load(
    const std::filesystem::path& project_dir,
    const std::filesystem::path& share_dir
) -> config;

} // namespace baldr
