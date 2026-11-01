/**
 * Part of Utility Library
 *
 * @brief   Extended logging — a windowed log renderer.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-07-12
 * @updated     2026-10-31
 * @status      Prototype
 *
 * This module sets up the default `spdlog` logger from a few orthogonal
 * `settings_t`: the line format, the terminal view (plain stream, a scrolling
 * `window`, or a `digest` of one-liners), and a log file keeping the full
 * record. See `resolve()` for the environment variables.
 *
 * @synopsis
 *
 * - `window` for keeping the buffer.
 * - `window_sink` as a `spdlog` adapter using `window`.
 * - `digest_sink` for minimal logging. Primary use-case is for agents to
 *   minimize context.
 *
 */

#pragma once

#include <cstddef>
#include <deque>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_set>

#include <fmt/format.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include <libutl/tty.hpp>

namespace utl::log {

/**
 * @brief   The prefix of a log line.
 */
enum class format_e {
    plain,       ///< The message only.
    timestamped, ///< `[2026-10-28 14:03:12.123456 +01:00] [name @thread] [level] message`
    epoch        ///< `1761656592.123456 L message`, `L` being the short level.
};

/**
 * @brief   How log records are presented on the terminal.
 */
enum class view_e {
    stream, ///< Every record, appended.
    window, ///< A rolling, in-place window, replayed in full on failure.
    digest  ///< Warnings and errors as capped one-liners, repeats collapsed, then a summary.
};

/**
 * @brief   The orthogonal logging settings, see `resolve()`.
 */
struct settings_t {
    format_e terminal_format = format_e::plain;
    view_e terminal_view = view_e::stream;
    std::optional<std::filesystem::path> file_dir; ///< No log file when empty.
    format_e file_format = format_e::epoch;
    std::size_t window_size = 10;                  ///< `view_e::window` only.
    std::size_t line_max = 160;                    ///< `view_e::digest` only.

    auto operator==(const settings_t&) const -> bool = default;
};

using env_lookup = std::function<std::optional<std::string>(const std::string&)>;

/**
 * @brief   A fixed-size, in-place rolling window of the most recent lines,
 *          backed by a larger replay buffer.
 *
 * `msg()` renders only the last `visible_lines()` lines in-place at the
 * cursor, erasing and redrawing on every call. Internally, up to
 * `max_buffer()` lines are retained so that `failure()` can dump the fuller
 * history that led up to it.
 *
 * `success()`/`failure()` both terminate the window and clear the buffer,
 * so the next tracked task starts with a clean slate.
 */
class window {
public:

    /**
     * @brief   Set how many of the most recent lines are rendered on screen
     *          at once.
     */
    void visible_lines(std::size_t count) {
        m_visible_lines = count;
    }

    /**
     * @brief   Set how many lines are retained for replay.
     */
    void max_buffer(std::size_t count) {
        m_max_buffer = count;
    }

    void msg(const std::string& msg);
    void success(const std::string& msg = "");
    void failure(const std::string& msg = "");

private:
    std::size_t m_visible_lines = 10;
    std::size_t m_max_buffer = 1000;
    std::deque<std::string> m_buffer;

};

/**
 * @brief   A reusable spdlog sink that renders log records via a scrolling
 *          `window`.
 */
class window_sink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit window_sink(std::string logger_name)
        : m_logger_name(std::move(logger_name))
    {}

    /**
     * @brief   Collapse the rolling window with a final success message,
     *          discarding the buffered log history.
     *
     * Call this once the work being tracked by the sink completes
     * successfully.
     *
     * @param   msg     An optional final message to print in place of the
     *                  collapsed window.
     */
    void success(const std::string& msg = "");

    /**
     * @brief   Collapse the rolling window with a final failure message.
     *
     * Call this explicitly once the work being tracked by the sink is known
     * to have failed.
     *
     * @param   msg     An optional final message to print in place of the
     *                  collapsed window.
     */
    void failure(const std::string& msg = "");

    /**
     * @brief   Access the underlying rolling window, e.g. to configure
     *          `visible_lines()`/`max_buffer()`.
     */
    window& scroll_window() {
        return m_window;
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    auto format_now(spdlog::level::level_enum level, const std::string& msg) -> std::string;

    std::string m_logger_name;
    window m_window;

};

/**
 * @brief   A spdlog sink printing warnings and errors as one-liners.
 *
 * Records below `warn` are dropped. Each line is stripped of ANSI escapes,
 * capped to `line_max` characters, and printed only the first time it occurs.
 *
 * `success()`/`failure()` print a summary with the counts and the log file
 * (with the complete log) path, then reset the counts.
 */
class digest_sink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit digest_sink(std::size_t line_max)
        : m_line_max(line_max)
    {}

    void success(const std::string& msg = "");
    void failure(const std::string& msg = "");

    /**
     * @brief   Print paths under `root` relative to it, saving the line budget
     *          for the message.
     */
    void relative_to(const std::filesystem::path& root);

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override;
    void flush_() override;

private:
    /**
     * @brief   What the summary reports since the last `success()`/`failure()`.
     */
    struct stats_t {
        std::size_t errors = 0;
        std::size_t warnings = 0;
        std::size_t repeats = 0;
    };

    void summary(const std::string& msg);

    std::string m_root_prefix;
    std::size_t m_line_max;

    stats_t m_stats;
    std::unordered_set<std::string> m_seen;

};

/**
 * @brief   Resolve the settings of the logger `name` from the environment.
 *
 * Precedence, highest first:
 * - `<NAME>_LOG_FORMAT`, `<NAME>_LOG_VIEW`, `<NAME>_LOG_FILE` (a directory,
 *   empty for none), `<NAME>_LOG_FILE_FORMAT`, `<NAME>_LOG_WINDOW_SIZE`,
 *   `<NAME>_LOG_LINE_MAX`, named by `utl::env_name()`
 * - defaults: plain stream, epoch-formatted file in `/tmp/<name>-logs`
 *
 * Invalid values are reported and ignored. The level is not part of the
 * settings: `SPDLOG_LEVEL` stays in charge of it.
 *
 * @param   getenv  Environment lookup. Injected so that the result depends
 *                  only on the arguments: tests pass a map instead of
 *                  mutating the process-wide environment.
 */
[[nodiscard]] auto resolve(const std::string& name, const env_lookup& getenv) -> settings_t;

/**
 * @brief   Initialize the default spdlog logger `name` with `resolve()`d
 *          settings.
 */
void init(const std::string& name);

/**
 * @brief   Initialize the default spdlog logger `name` with `config`.
 *
 * No log file is opened yet, see `open_file()`.
 */
void init(const std::string& name, const settings_t& config);

/**
 * @brief   Start writing the full log to `<file_dir>/<YYYYMMDDTHHMMSS>_<label>.log`.
 *
 * The file receives at least `info` records, whatever the terminal level is.
 * A file that can't be opened is reported, and logging carries on without it.
 *
 * @param   label   Identifies the run in the file name, e.g. `<project>_<command>`.
 */
void open_file(const std::string& label);

/**
 * @brief   The log file opened by `open_file()`, if any.
 */
[[nodiscard]] auto file_path() -> std::optional<std::filesystem::path>;

/**
 * @brief   Shorten paths under `root` in the `digest_sink` of `logger_name`
 *          (default logger when empty), if it has one.
 */
void relative_to(const std::filesystem::path& root, const std::string& logger_name = "");

/**
 * @brief   Environment for a child process that is itself a `utl::log` user
 *          (e.g. a nested `baldr`), so that it streams plain lines for this
 *          process to re-log and writes no log file of its own.
 */
[[nodiscard]] auto child_env() -> std::map<std::string, std::string>;

/**
 * @brief   Signal that the work tracked by a logger completed successfully,
 *          collapsing the rolling window if one is active.
 *
 * Looks up the `window_sink` or `digest_sink` attached to the logger named
 * `logger_name` (`spdlog::default_logger()` when empty) and calls its
 * `success(msg)`. In `view_e::stream`, this instead logs `msg` at `info` level,
 * so callers don't need to know which view is active.
 *
 * @param   msg             An optional final message to print/log.
 * @param   logger_name     Name of the topic logger to target, or empty to
 *                          use `spdlog::default_logger()`.
 */
void success(const std::string& msg = "", const std::string& logger_name = "");

/**
 * @brief   Signal that the work tracked by a logger failed, collapsing the
 *          rolling window if one is active.
 *
 * Mirrors `success()`; in `view_e::stream`, this logs `msg` at `error` level.
 *
 * @param   msg             An optional final message to print/log.
 * @param   logger_name     Name of the topic logger to target, or empty to
 *                          use `spdlog::default_logger()`.
 */
void failure(const std::string& msg = "", const std::string& logger_name = "");

} // namespace utl::log
