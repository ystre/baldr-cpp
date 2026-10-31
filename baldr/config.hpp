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
 * @status      Clean
 */

#pragma once

#include <libnova/dyn_value.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
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
};

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
 * @brief   The config file in `dir`: `.baldr.yaml`, `.baldr.yml`, `.baldr.json`
 *          or `.baldr.toml`.
 *
 * @returns empty if there is none.
 *
 * @throws  if there is more than one, or `dir` can't be searched.
 */
[[nodiscard]] auto find_config(const std::filesystem::path& dir) -> std::optional<std::filesystem::path>;

/**
 * @brief   Read the config file in `dir`, see `find_config`.
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
 * The global config (in `$HOME`) merged with the project-local one (in
 * `project_dir`). Each can be in any format, independently of the other.
 *
 * @throws  if a config file fails to load.
 */
[[nodiscard]] auto load(const std::filesystem::path& project_dir) -> config;

} // namespace baldr
