/**
 * Part of Baldr
 *
 * The builder. Contains the business logic.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 */

#pragma once

#include <baldr/config.hpp>

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace baldr {

enum class project_type {
    make,
    cmake
};

/**
 * @brief   Wraps a project's build invocation via `baldr::command`.
 */
class builder {
public:
    /**
     * @brief   Construct a `builder` for `project_dir`, sourcing build
     *          type, CMake defines/env and debugger settings from `cfg`.
     *
     * @param   project_dir         Project directory to build/run in.
     * @param   cfg                 Resolved configuration (already merged
     *                              with any CLI overrides by the caller).
     * @param   build_dir_override  If set, used instead of the default
     *                              `build/<build_type>` build directory
     *                              (relative to `project_dir`).
     */
    builder(
        std::string project_dir = ".",
        config cfg = {},
        std::optional<std::string> build_dir_override = std::nullopt
    );

    /**
     * @brief   Run the build command inside `project_dir`, streaming its
     *          combined stdout/stderr output via `nova::log::info`.
     *
     * @param   clean_build     If `true`, wipe the resolved build directory
     *                          (CMake) or run `make clean` (Makefile, if a
     *                          `clean` target exists) before building.
     *
     * @throws  nova::exception if the build command exits with a non-zero
     *          code.
     */
    void build(const std::string& target = "", bool clean_build = false);

    /**
     * @brief   Run `target`, attached to the caller's own TTY.
     *
     * @param   target          Executable name (relative to `project_dir`)
     *                          to run.
     * @param   forwarded_args  Extra arguments appended after `target`'s own
     *                          path, forwarded verbatim to its argv (e.g.
     *                          everything following a literal `--` on
     *                          baldr's own command line).
     * @param   debug           If `true`, launch `target` under the
     *                          configured debugger (`m_debugger`/
     *                          `m_debugger_args`) instead of running it
     *                          directly.
     *
     * @throws  nova::exception if `target` exits with a non-zero code.
     */
    void run(const std::string& target, const std::vector<std::string>& forwarded_args = {}, bool debug = false);

    /**
     * @brief   Run an arbitrary executable (script or binary), attached to
     *          the caller's own TTY, bypassing target resolution/build
     *          entirely.
     *
     * The executable is exposed to Baldr's directory conventions via
     * `BALDR_ENV_WORKING_DIR` (absolute project directory) and
     * `BALDR_ENV_BUILD_DIR` (build directory, relative to the project
     * directory, for the configured build type). This is a separate env set
     * from `m_cmake_env` (CC/CXX etc. for the build); it is hardcoded for
     * now, but is expected to become config-sourced (`.baldr.yaml`) rather
     * than fixed to just these two variables.
     *
     * Wall-clock elapsed time, CPU time and peak memory (via `wait4(2)`,
     * see `command::exit_status::usage()`) are measured around the run and
     * reported together with the exit code.
     *
     * @param   exec_path       Path to the executable to run, resolved
     *                          relative to `project_dir` if not absolute.
     * @param   forwarded_args  Extra arguments appended after the
     *                          executable's own path, forwarded verbatim to
     *                          its argv.
     * @param   debug           If `true`, launch it under the configured
     *                          debugger, same as `run()` (it's on the caller
     *                          not to point a debugger at a shell script).
     *
     * @throws  nova::exception if the executable doesn't exist or exits with
     *          a non-zero code.
     */
    void run_exec(const std::string& exec_path, const std::vector<std::string>& forwarded_args = {}, bool debug = false);

private:
    std::string m_project_dir;
    std::string m_build_type;
    std::optional<std::string> m_build_dir_override;
    std::map<std::string, std::string> m_cmake_defines;
    std::map<std::string, std::string> m_cmake_env;
    std::string m_debugger;
    std::vector<std::string> m_debugger_args;
    project_type m_project_type { project_type::make };

    void discover_project_type();

    [[nodiscard]] auto resolve_conan_provider() const -> std::optional<std::string>;
    [[nodiscard]] auto effective_build_dir_rel() const -> std::string;
    [[nodiscard]] auto handle_project(const std::string& target = "", bool clean_build = false) -> std::vector<std::string>;
    [[nodiscard]] auto resolve_executable(const std::string& target) const -> std::string;
    [[nodiscard]] auto handle_makefile_project(bool clean_build) const -> std::vector<std::string>;
    [[nodiscard]] auto handle_cmake_project(const std::string& target, bool clean_build) const -> std::vector<std::string>;

    void configure_cmake(
        const std::filesystem::path& build_dir,
        const std::string& build_dir_rel,
        const std::string& resolved_defines,
        const std::optional<std::string>& conan_provider
    ) const;

    [[nodiscard]] auto build_argv(
        const std::string& exe_path,
        const std::vector<std::string>& forwarded_args,
        bool debug
    ) const -> std::vector<std::string>;

};

} // namespace baldr
