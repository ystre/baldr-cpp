/**
 * Part of Utility Library
 *
 * Scrollog — a scrolling, rolling-window log/progress renderer.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 *
 * This module contains a "scrolling log" `spdlog` sink, `scrollog`, which
 * builds upon the `window` serving as a ring-buffer.
 *
 * SPDLOG_MODE environment variable can be used to configure the behaviour:
 * - standard
 * - scrollog (alias: rlog)
 * - plain
 */

#pragma once

#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <string>

#include <fmt/format.h>
#include <spdlog/details/log_msg.h>
#include <spdlog/sinks/base_sink.h>

#include <libutl/tty.hpp>

namespace utl::rlog {

static constexpr auto EnvLogMode = "SPDLOG_MODE";
static constexpr auto EnvWindowSize = "RLOG_WINDOW_SIZE";

enum class mode {
    standard, ///< Timestamped, appendable log lines.
    progress, ///< In-place, overwritable log lines presented in a rolling window.
    plain     ///< Appendable log lines with no timestamp/name/level prefix, just the message.
};

/**
 * @brief   A fixed-size, in-place rolling window of the most recent lines,
 *          backed by a larger replay buffer.
 *
 * `msg()` renders only the last `visible_lines()` lines in-place at the
 * cursor, erasing and redrawing on every call. Internally, up to
 * `max_buffer()` lines are retained (independently of how many are
 * currently visible on screen) so that `failure()` can dump the fuller
 * history that led up to it, not just what happened to be on screen.
 * `success()`/`failure()` both terminate the window and clear the buffer,
 * so the next tracked task starts with a clean slate.
 */
class window {
public:
    /**
     * @brief   Set how many of the most recent lines are rendered on screen
     *          at once.
     */
    void visible_lines(std::size_t n) {
        m_visible_lines = n;
    }

    /**
     * @brief   Set how many lines are retained for replay (e.g. via
     *          `failure()`), independently of `visible_lines()`.
     */
    void max_buffer(std::size_t n) {
        m_max_buffer = n;
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
class scrollog_sink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit scrollog_sink(std::string logger_name) : m_logger_name(std::move(logger_name)) { }

    /**
     * @brief   Collapse the rolling window with a final success message,
     *          discarding the buffered log history.
     *
     * Call this once the work being tracked by the sink completes
     * successfully, e.g. after a build finishes without errors. The message
     * is formatted through the sink's own `formatter_`, so it carries the
     * same timestamp/pattern as regular log records without recursing back
     * through spdlog's logging dispatch.
     *
     * @param   msg     An optional final message to print in place of the
     *                  collapsed window.
     */
    void success(const std::string& msg = "");

    /**
     * @brief   Collapse the rolling window with a final failure message,
     *          dumping the entire buffered history so nothing that led up
     *          to the failure is lost.
     *
     * Call this explicitly once the work being tracked by the sink is known
     * to have failed — this is never triggered automatically by logging an
     * `error`/`critical` record. The message is formatted the same way as
     * `success()`.
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
 * @brief   Initialize the default spdlog logger with the sink matching `m`.
 *
 * When `m` is not given, the effective mode is resolved with the following
 * precedence: `SPDLOG_MODE` environment variable > `isatty(stderr)`
 * auto-detection > `mode::standard` default.
 *
 * In `mode::progress`, the number of visible lines rendered by the rolling
 * window can be configured via the `RLOG_WINDOW_SIZE` environment variable
 * (a positive integer); invalid or missing values fall back to the
 * `window` default.
 *
 * `mode::plain` exists for processes whose own stdout/stderr is being
 * captured and re-logged by a *parent* process that already attaches its
 * own timestamp/name/level prefix
 *
 * @param   m       The logging mode to initialize with, or `std::nullopt` to
 *                  auto-resolve it.
 * @param   name    Logger name (also used as the topic name).
 */
void init(const std::string& name, std::optional<mode> m = std::nullopt);

/**
 * @brief   Signal that the work tracked by a logger completed successfully,
 *          collapsing the rolling window if one is active.
 *
 * Looks up the `scrollog_sink` attached to the logger named `logger_name`
 * (`spdlog::default_logger()` when empty, as set up by `init()` in
 * `mode::progress`) and calls its `success(msg)`. In `mode::standard` (no
 * `scrollog_sink` attached), this instead logs `msg` at `info` level, so
 * callers don't need to know which mode is active.
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
 * Mirrors `success()`: looks up the `scrollog_sink` attached to the logger
 * named `logger_name` (`spdlog::default_logger()` when empty) and calls its
 * `failure(msg)`. In `mode::standard` (no `scrollog_sink` attached), this
 * instead logs `msg` at `error` level.
 *
 * @param   msg             An optional final message to print/log.
 * @param   logger_name     Name of the topic logger to target, or empty to
 *                          use `spdlog::default_logger()`.
 */
void failure(const std::string& msg = "", const std::string& logger_name = "");

} // namespace utl::rlog
