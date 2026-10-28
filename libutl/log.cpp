/**
 * Part of Utility Library
 *
 * @brief   Extended logging — a windowed log renderer.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-07-12
 * @updated     2026-10-31
 * @status      Prototype
 */

#include <libutl/log.hpp>

#include <libutl/env.hpp>

#include <libnova/log.hpp>
#include <libnova/utils.hpp>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

#include <fmt/chrono.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace utl::log {

namespace {

/**
 * @brief   What `init()` and `open_file()` set up, for `child_env()` and
 *          `file_path()`.
 */
struct state_t {
    std::string name;
    settings_t config;
    std::optional<std::filesystem::path> file;
};

auto current() -> state_t& {
    static state_t instance;
    return instance;
}

/**
 * @brief   `<NAME>_LOG_<KEY>`, the variable configuring `key` of the logger `name`.
 */
[[nodiscard]] auto log_env_name(std::string_view name, std::string_view key) -> std::string {
    return env_name(name, fmt::format("LOG_{}", key));
}

[[nodiscard]] auto pattern_of(format_e format) -> std::string {
    switch (format) {
        case format_e::plain:         return "%v";
        case format_e::timestamped:   return "[%Y-%m-%d %H:%M:%S.%f %z] [%n @%t] %^[%l]%$ %v";
        case format_e::epoch:         return "%E.%f %L %v";
    }
    std::unreachable();
}

[[nodiscard]] auto parse_format(std::string_view value) -> std::optional<format_e> {
    if (value == "plain")       { return format_e::plain; }
    if (value == "timestamped") { return format_e::timestamped; }
    if (value == "epoch")       { return format_e::epoch; }
    return std::nullopt;
}

[[nodiscard]] auto parse_view(std::string_view value) -> std::optional<view_e> {
    if (value == "stream")      { return view_e::stream; }
    if (value == "window")      { return view_e::window; }
    if (value == "digest")      { return view_e::digest; }
    return std::nullopt;
}

[[nodiscard]] auto parse_positive(const std::string& value) -> std::optional<std::size_t> {
    try {
        std::size_t consumed = 0;
        const long long parsed = std::stoll(value, &consumed);

        if (consumed != value.size() or parsed <= 0) {
            return std::nullopt;
        }

        return static_cast<std::size_t>(parsed);
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

/**
 * @brief   Apply the environment variable `var`, if set, through `parse`,
 *          reporting a value `parse` rejects.
 */
template <typename Parse, typename Apply>
void apply_env(const env_lookup& getenv, const std::string& var, Parse parse, Apply apply) {
    auto value = getenv(var);

    if (not value.has_value()) {
        return;
    }

    if (auto parsed = parse(*value); parsed.has_value()) {
        apply(*parsed);
    } else {
        nova::log::warn("Invalid {} value '{}', ignoring", var, *value);
    }
}

/**
 * @brief   Resolve `logger_name` to a logger (`spdlog::default_logger()` when
 *          empty), then return the first `Sink` attached to it, or `nullptr`.
 */
template <typename Sink>
auto find_sink(const std::string& logger_name) -> std::shared_ptr<Sink> {
    auto logger = logger_name.empty()
        ? spdlog::default_logger()
        : spdlog::get(logger_name);

    if (not logger) {
        return nullptr;
    }

    for (auto& sink: logger->sinks()) {
        if (auto found = std::dynamic_pointer_cast<Sink>(sink)) {
            return found;
        }
    }

    return nullptr;
}

/**
 * @brief   Log `msg` at `level` on `logger_name` (default logger when empty).
 */
void log_to(const std::string& logger_name, spdlog::level::level_enum level, const std::string& msg) {
    auto logger = logger_name.empty()
        ? spdlog::default_logger()
        : spdlog::get(logger_name);

    if (logger) {
        logger->log(level, msg);
    }
}

/**
 * @brief   A file sink writing messages without their ANSI escapes.
 */
class plain_file_sink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit plain_file_sink(const std::filesystem::path& path) {
        m_file.open(path.string(), /*truncate=*/false);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        const auto payload = tty::strip_ansi(std::string_view{ msg.payload.data(), msg.payload.size() });
        auto stripped = msg;
        stripped.payload = payload;

        spdlog::memory_buf_t formatted;
        formatter_->format(stripped, formatted);
        m_file.write(formatted);
    }

    void flush_() override {
        m_file.flush();
    }

private:
    spdlog::details::file_helper m_file;

};

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
auto window_sink::format_now(spdlog::level::level_enum level, const std::string& msg) -> std::string {
    spdlog::details::log_msg record(m_logger_name, level, msg);
    spdlog::memory_buf_t formatted;
    formatter_->format(record, formatted);
    return strip_trailing_newline(fmt::to_string(formatted));
}

void window_sink::success(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    m_window.success(
        msg.empty()
            ? msg
            : format_now(spdlog::level::info, msg)
    );
}

void window_sink::failure(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    m_window.failure(
        msg.empty()
            ? msg
            : format_now(spdlog::level::err, msg)
    );
}

void window_sink::sink_it_(const spdlog::details::log_msg& msg) {
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string line = strip_trailing_newline(fmt::to_string(formatted));

    m_window.msg(line);
}

void window_sink::flush_() {
    std::fflush(stdout);
}

void digest_sink::sink_it_(const spdlog::details::log_msg& msg) {
    if (msg.level < spdlog::level::warn) {
        return;
    }

    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);

    auto line = tty::strip_ansi(strip_trailing_newline(fmt::to_string(formatted)));
    if (not m_root_prefix.empty()) {
        for (auto pos = line.find(m_root_prefix); pos != std::string::npos; pos = line.find(m_root_prefix, pos)) {
            line.erase(pos, m_root_prefix.size());
        }
    }

    line = tty::cap_visible_width(line, m_line_max);

    if (not m_seen.insert(line).second) {
        ++m_stats.repeats;
        return;
    }

    if (msg.level >= spdlog::level::err) {
        ++m_stats.errors;
    } else {
        ++m_stats.warnings;
    }

    fmt::println(stderr, "{}", line);
}

void digest_sink::flush_() {
    std::fflush(stderr);
}

void digest_sink::summary(const std::string& msg) {
    std::vector<std::string> parts;

    if (m_stats.errors > 0) {
        parts.push_back(fmt::format("{} error{}", m_stats.errors, m_stats.errors == 1 ? "" : "s"));
    }

    if (m_stats.warnings > 0) {
        parts.push_back(fmt::format("{} warning{}", m_stats.warnings, m_stats.warnings == 1 ? "" : "s"));
    }

    if (m_stats.repeats > 0) {
        parts.push_back(fmt::format("{} repeated", m_stats.repeats));
    }

    if (auto file = file_path()) {
        parts.push_back(fmt::format("full log: {}", file->string()));
    }

    if (not msg.empty() or not parts.empty()) {
        fmt::println(stderr, "{}{}{}", msg, msg.empty() or parts.empty() ? "" : " ", fmt::join(parts, ", "));
    }

    m_stats = {};
    m_seen.clear();
}

void digest_sink::relative_to(const std::filesystem::path& root) {
    std::lock_guard<std::mutex> lock(mutex_);
    m_root_prefix = root.empty() ? "" : (root / "").string();
}

void digest_sink::success(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    summary(msg);
}

void digest_sink::failure(const std::string& msg) {
    std::lock_guard<std::mutex> lock(mutex_);
    summary(msg);
}

auto resolve(const std::string& name, const env_lookup& getenv) -> settings_t {
    settings_t config;
    config.file_dir = std::filesystem::temp_directory_path() / fmt::format("{}-logs", name);

    apply_env(getenv, log_env_name(name, "FORMAT"), parse_format, [&](format_e format) { config.terminal_format = format; });
    apply_env(getenv, log_env_name(name, "VIEW"), parse_view, [&](view_e view) { config.terminal_view = view; });
    apply_env(getenv, log_env_name(name, "FILE_FORMAT"), parse_format, [&](format_e format) { config.file_format = format; });
    apply_env(getenv, log_env_name(name, "WINDOW_SIZE"), parse_positive, [&](std::size_t lines) { config.window_size = lines; });
    apply_env(getenv, log_env_name(name, "LINE_MAX"), parse_positive, [&](std::size_t length) { config.line_max = length; });

    if (auto dir = getenv(log_env_name(name, "FILE")); dir.has_value()) {
        config.file_dir = dir->empty()
            ? std::nullopt
            : std::optional<std::filesystem::path>{ *dir };
    }

    return config;
}

void init(const std::string& name) {
    init(name, resolve(name, [](const std::string& var) -> std::optional<std::string> {
        if (auto value = nova::getenv(var); value.has_value()) {
            return *value;
        }
        return std::nullopt;
    }));
}

void init(const std::string& name, const settings_t& config) {
    nova::log::load_env_levels();
    spdlog::drop(name);

    std::shared_ptr<spdlog::logger> logger;
    switch (config.terminal_view) {
        case view_e::stream:
            logger = spdlog::create<spdlog::sinks::ansicolor_stderr_sink_mt>(name);
            break;
        case view_e::window:
            logger = spdlog::create<window_sink>(name, name);
            if (auto sink = std::dynamic_pointer_cast<window_sink>(logger->sinks().front())) {
                sink->scroll_window().visible_lines(config.window_size);
            }
            break;
        case view_e::digest:
            logger = spdlog::create<digest_sink>(name, config.line_max);
            break;
    }

    logger->set_pattern(pattern_of(config.terminal_format));
    spdlog::set_default_logger(logger);

    current() = state_t{ .name = name, .config = config, .file = std::nullopt };
}

void open_file(const std::string& label) {
    auto& state = current();
    if (not state.config.file_dir.has_value() or state.file.has_value()) {
        return;
    }

    // TODO(design): Nothing removes old files; `/tmp` is the only cleanup, a long-lived
    //               machine or a custom `<NAME>_LOG_FILE` grows without bound.
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const auto path = *state.config.file_dir / fmt::format("{:%Y%m%dT%H%M%SZ}_{}.log", now, label);

    std::shared_ptr<plain_file_sink> file_sink;
    try {
        file_sink = std::make_shared<plain_file_sink>(path);
    } catch (const spdlog::spdlog_ex& error) {
        nova::log::warn("No log file: {}", error.what());
        return;
    }

    file_sink->set_pattern(pattern_of(state.config.file_format));

    auto logger = spdlog::default_logger();
    const auto terminal_level = logger->level();
    const auto file_level = std::min(terminal_level, spdlog::level::info);

    for (auto& sink: logger->sinks()) {
        sink->set_level(terminal_level);
    }

    file_sink->set_level(file_level);
    logger->sinks().push_back(file_sink);
    logger->set_level(file_level);

    state.file = path;
}

auto file_path() -> std::optional<std::filesystem::path> {
    return current().file;
}

void relative_to(const std::filesystem::path& root, const std::string& logger_name) {
    if (auto digest = find_sink<digest_sink>(logger_name)) {
        digest->relative_to(root);
    }
}

auto child_env() -> std::map<std::string, std::string> {
    const auto& name = current().name;
    return {
        { log_env_name(name, "FORMAT"), "plain" },
        { log_env_name(name, "VIEW"), "stream" },
        { log_env_name(name, "FILE"), "" },
    };
}

void success(const std::string& msg, const std::string& logger_name) {
    if (auto window = find_sink<window_sink>(logger_name)) {
        window->success(msg);
        return;
    }

    if (auto digest = find_sink<digest_sink>(logger_name)) {
        digest->success(msg);
        return;
    }

    if (not msg.empty()) {
        log_to(logger_name, spdlog::level::info, msg);
    }
}

void failure(const std::string& msg, const std::string& logger_name) {
    if (auto window = find_sink<window_sink>(logger_name)) {
        window->failure(msg);
        return;
    }

    if (auto digest = find_sink<digest_sink>(logger_name)) {
        digest->failure(msg);
        return;
    }

    if (not msg.empty()) {
        log_to(logger_name, spdlog::level::err, msg);
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
    for (const auto& line : std::ranges::subrange(visible_begin, m_buffer.end())) {
        fmt::println("{}", tty::cap_visible_width(line, max_width));
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

} // namespace utl::log
