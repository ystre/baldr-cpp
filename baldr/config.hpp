/**
 * Part of Baldr
 *
 * Configuration loading.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-19
 */

#pragma once

#include <libnova/expected.hpp>
#include <libnova/error.hpp>
#include <libnova/yaml.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace baldr {

/**
 * @brief   A single named `batch` stage: a shell command, plus the names of
 *          the parameters it expects to be exposed to that command as
 *          environment variables (see `batch_step::overrides`).
 */
struct batch_stage {
    std::string cmd;
    std::vector<std::string> parameters;
};

/**
 * @brief   One step of a `batch` scenario: the stage to run, and any
 *          `@key=value` parameter overrides given alongside it (keys already
 *          normalized `-` -> `_`, see `parse_step()`).
 */
struct batch_step {
    std::string stage;
    std::map<std::string, std::string> overrides;
};

/**
 * @brief   The `batch:` section of a `.baldr.yaml`: shared environment,
 *          named stages (single shell commands) and named scenarios
 *          (ordered lists of stage invocations).
 */
struct batch_config {
    std::map<std::string, std::string> env;
    std::map<std::string, batch_stage> stages;
    std::map<std::string, std::vector<batch_step>> scenarios;
};

/**
 * @brief   Parse one `batch` scenario step, e.g. `"install @build-type=Release"`,
 *          into its stage name and `key=value` overrides. Override keys have
 *          `-` normalized to `_`, matching `batch_stage::parameters` naming
 *          (mirrors baldr's own `--build-type` CLI-flag-to-`build_type`
 *          convention).
 *
 * @throws  nova::exception if `step` is empty, or an `@`-token isn't a valid
 *          `key=value` pair.
 */
[[nodiscard]] auto parse_step(const std::string& step) -> batch_step;

/**
 * @brief   Per-project settings, loaded from a `.baldr.yaml` (project-local)
 *          or `~/.baldr.yaml` (global) config file.
 */
struct config {
    std::string debugger = "gdb";
    std::vector<std::string> debugger_args = { "--args" };
    std::string build_type = "Debug";
    std::map<std::string, std::string> cmake_defines;
    std::map<std::string, std::string> env;

    bool docker_mount_baldr = true;
    std::optional<std::string> docker_baldr_path;

    batch_config batch;
};

/**
 * @brief   Parse a `config` out of the YAML `doc`, keeping already
 *          initialized defaults for anything not present in `doc`.
 */
[[nodiscard]] auto parse(const nova::yaml& doc, config result) -> config;

/**
 * @brief   Parse `yaml_content` on top of `result`.
 *
 * @return  `result` merged with anything present in `yaml_content`, or an
 *          error if `yaml_content` is malformed.
 */
[[nodiscard]] auto
load(std::string_view yaml_content, config result) -> nova::expected<config, nova::error>;

/**
 * @brief   Parse a `.baldr.yaml` file at `path` on top of `result`, if it
 *          exists.
 *
 * @return  `result` unchanged if `path` doesn't exist, the merged config on
 *          success, or an error if the file exists but is malformed.
 */
[[nodiscard]] auto
load(const std::filesystem::path& path, baldr::config result) -> nova::expected<baldr::config, nova::error>;

/**
 * @brief   Load the config for `project_dir`.
 *
 * Falls back to global config (`$HOME/.baldr.yaml`) if there is no project
 * configuration.
 *
 * @param   project_dir     Project directory to look for a project-local
 *                          config in.
 *
 * @return  Loaded (or default) config, or an error if a found config
 *          file could not be parsed.
 */
[[nodiscard]] auto load(const std::filesystem::path& project_dir) -> nova::expected<config, nova::error>;

} // namespace baldr
