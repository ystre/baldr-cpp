#include <libutl/rlog.hpp>

#include <libnova/log.hpp>
#include <libnova/utils.hpp>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <unistd.h>

namespace utl::rlog {

namespace {

/**
 * @brief   Resolve the effective logging mode when none is given explicitly.
 *
 * Precedence: `SPDLOG_MODE` environment variable > `isatty(stderr)`
 * auto-detection > `mode::standard` default.
 *
 * SPDLOG_MODE:
 * - standard
 * - scrollog (alias: rlog)
 * - plain
 */
auto resolve_mode() -> mode {
    if (auto mode = nova::getenv(EnvLogMode); mode.has_value()) {
        if (*mode == "standard") {
            return mode::standard;
        }
        if (*mode == "scrollog" or *mode == "rlog") {
            return mode::progress;
        }
        if (*mode == "plain") {
            return mode::plain;
        }
        nova::log::warn("Invalid {} value '{}', falling back to 'standard'", EnvLogMode, *mode);
        return mode::standard;
    }

    if (isatty(fileno(stderr)) != 0) {
        return mode::progress;
    }

    return mode::standard;
}

/**
 * @brief   Resolve the configured window size from the `RLOG_WINDOW_SIZE`
 *          environment variable, if set to a valid positive integer.
 */
auto resolve_window_size() -> std::optional<std::size_t> {
    auto window_size = nova::getenv(EnvWindowSize);
    if (not window_size.has_value()) {
        return std::nullopt;
    }

    try {
        const int value = std::stoi(*window_size);
        if (value <= 0) {
            nova::log::warn("Invalid {} value '{}', ignoring", EnvWindowSize, *window_size);
            return std::nullopt;
        }
        return static_cast<std::size_t>(value);
    } catch (const std::exception&) {
        nova::log::warn("Invalid {} value '{}', ignoring", EnvWindowSize, *window_size);
        return std::nullopt;
    }
}

/**
 * @brief   Resolve `logger_name` to a logger (`spdlog::default_logger()` when
 *          empty), then return the first `scrollog_sink` attached to it, or
 *          `nullptr` if there is none.
 *
 * A logger is assumed to carry at most one `scrollog_sink`.
 */
auto find_scrollog_sink(const std::string& logger_name) -> std::shared_ptr<scrollog_sink> {
    auto logger = logger_name.empty()
        ? spdlog::default_logger()
        : spdlog::get(logger_name);

    if (not logger) {
        return nullptr;
    }

    for (auto& sink: logger->sinks()) {
        if (auto scrollog = std::dynamic_pointer_cast<scrollog_sink>(sink)) {
            return scrollog;
        }
    }

    return nullptr;
}

} // namespace

/**
 * @brief   Strip any trailing newline spdlog's formatter appended.
 */
auto strip_trailing_newline(std::string line) -> std::string {
    while (not line.empty() and (line.back() == '\n' or line.back() == '\r')) {
        line.pop_back();
    }
    return line;
}

/**
 * @brief   Build a synthetic `log_msg` and format it directly
 *
 * This avoids recursing back into `sink_it_` while still reusing the exact same
 * timestamp/pattern as regular records logged through this sink.
 */
auto scrollog_sink::format_now(spdlog::level::level_enum level, const std::string& msg) -> std::string {
    spdlog::details::log_msg record(m_logger_name, level, msg);
    spdlog::memory_buf_t formatted;
    formatter_->format(record, formatted);
    return strip_trailing_newline(fmt::to_string(formatted));
}

void scrollog_sink::success(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    m_window.success(
        msg.empty()
            ? msg
            : format_now(spdlog::level::info, msg)
    );
}

void scrollog_sink::failure(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    m_window.failure(
        msg.empty()
            ? msg
            : format_now(spdlog::level::err, msg)
    );
}

void scrollog_sink::sink_it_(const spdlog::details::log_msg& msg) {
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string line = strip_trailing_newline(fmt::to_string(formatted));

    m_window.msg(line);
}

void scrollog_sink::flush_() {
    std::fflush(stdout);
}

void init(const std::string& name, std::optional<mode> m) {
    nova::log::load_env_levels();
    spdlog::drop(name);

    mode resolved = m.value_or(resolve_mode());
    if (resolved == mode::progress) {
        auto logger = spdlog::create<scrollog_sink>(name, name);
        if (auto sink = std::dynamic_pointer_cast<scrollog_sink>(logger->sinks().front())) {
            if (auto window_size = resolve_window_size()) {
                sink->scroll_window().visible_lines(*window_size);
            }
        }
        spdlog::set_default_logger(logger);
    } else {
        spdlog::set_default_logger(spdlog::create<spdlog::sinks::ansicolor_stderr_sink_mt>(name));
    }

    spdlog::default_logger()->set_pattern(
        resolved == mode::plain
            ? "%v"
            : "[%Y-%m-%d %H:%M:%S.%f %z] [%n @%t] %^[%l]%$ %v"
    );
}

void success(const std::string& msg, const std::string& logger_name) {
    if (auto scrollog = find_scrollog_sink(logger_name)) {
        scrollog->success(msg);
        return;
    }

    if (not msg.empty()) {
        auto logger = logger_name.empty()
            ? spdlog::default_logger()
            : spdlog::get(logger_name);

        if (logger) {
            logger->info(msg);
        }
    }
}

void failure(const std::string& msg, const std::string& logger_name) {
    if (auto scrollog = find_scrollog_sink(logger_name)) {
        scrollog->failure(msg);
        return;
    }

    if (not msg.empty()) {
        auto logger = logger_name.empty()
            ? spdlog::default_logger()
            : spdlog::get(logger_name);

        if (logger) {
            logger->error(msg);
        }
    }
}

void window::msg(const std::string& msg) {
    if (m_buffer.size() == m_max_buffer) {
        m_buffer.pop_front();
    }

    m_buffer.push_back(msg);

    tty::erase_to_end();

    std::size_t visible_count = std::min(m_visible_lines, m_buffer.size());
    auto visible_begin = m_buffer.end() - static_cast<std::ptrdiff_t>(visible_count);

    std::size_t max_width = tty::terminal_width() > 0 ? tty::terminal_width() - 1 : 0;
    for (auto it = visible_begin; it != m_buffer.end(); ++it) {
        std::string capped = tty::cap_visible_width(*it, max_width);
        fmt::println("{}", capped);
    }

    tty::cursor_up(visible_count);
}

void window::success(const std::string& msg) {
    tty::erase_to_end();

    if (not msg.empty()) {
        fmt::println("\033[1;34m{}\033[0m", msg);
    }

    m_buffer.clear();
}

void window::failure(const std::string& msg) {
    tty::erase_to_end();

    for (auto& line: m_buffer) {
        fmt::println("{}", line);
    }

    if (not msg.empty()) {
        fmt::println("\033[1;31m{}\033[0m", msg);
    }

    m_buffer.clear();
}

} // namespace utl::rlog
