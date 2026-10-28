#include <libutl/rlog.hpp>

#include <libnova/log.hpp>
#include <libnova/utils.hpp>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/details/file_helper.h>
#include <spdlog/spdlog.h>
#include <spdlog/pattern_formatter.h>

#include <fmt/chrono.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>


namespace utl::rlog {

namespace {

/**
 * @brief   What `init()` and `open_file()` set up, for `child_env()` and
 *          `file_path()`.
 */
struct state {
    std::string name;
    settings config;
    std::optional<std::filesystem::path> file;
};

auto current() -> state& {
    static state instance;
    return instance;
}

[[nodiscard]] auto pattern_of(format f) -> std::string {
    switch (f) {
        case format::plain:         return "%v";
        case format::timestamped:   return "[%Y-%m-%d %H:%M:%S.%f %z] [%n @%t] %^[%l]%$ %v";
        case format::epoch:         return "%E.%f %L %v";
    }
    std::unreachable();
}

[[nodiscard]] auto parse_format(std::string_view value) -> std::optional<format> {
    if (value == "plain")       { return format::plain; }
    if (value == "timestamped") { return format::timestamped; }
    if (value == "epoch")       { return format::epoch; }
    return std::nullopt;
}

[[nodiscard]] auto parse_view(std::string_view value) -> std::optional<view> {
    if (value == "stream")      { return view::stream; }
    if (value == "window")      { return view::window; }
    if (value == "digest")      { return view::digest; }
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
 * @brief   Apply the deprecated `SPDLOG_MODE`, which sets format and view at once.
 */
void apply_legacy_mode(const env_lookup& getenv, settings& s) {
    apply_env(getenv, EnvLogMode,
        [](const std::string& value) -> std::optional<std::pair<format, view>> {
            if (value == "standard")                        { return std::pair{ format::timestamped, view::stream }; }
            if (value == "rlog" or value == "scrollog")     { return std::pair{ format::timestamped, view::window }; }
            if (value == "plain")                           { return std::pair{ format::plain, view::stream }; }
            return std::nullopt;
        },
        [&](std::pair<format, view> mode) {
            s.terminal_format = mode.first;
            s.terminal_view = mode.second;
        }
    );
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
 * @brief   A file sink writing messages without their ANSI escapes, so the
 *          file stays greppable whatever colors the subprocesses emitted.
 */
class plain_file_sink : public spdlog::sinks::base_sink<std::mutex> {
public:
    explicit plain_file_sink(const std::filesystem::path& path) {
        m_file.open(path.string(), /*truncate=*/false);
    }

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        const auto payload = tty::strip_ansi(std::string{ msg.payload.data(), msg.payload.size() });
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
        ++m_repeats;
        return;
    }

    if (msg.level >= spdlog::level::err) {
        ++m_errors;
    } else {
        ++m_warnings;
    }
    fmt::println(stderr, "{}", line);
}

void digest_sink::flush_() {
    std::fflush(stderr);
}

void digest_sink::summary(const std::string& msg) {
    std::vector<std::string> parts;
    if (m_errors > 0) {
        parts.push_back(fmt::format("{} error{}", m_errors, m_errors == 1 ? "" : "s"));
    }
    if (m_warnings > 0) {
        parts.push_back(fmt::format("{} warning{}", m_warnings, m_warnings == 1 ? "" : "s"));
    }
    if (m_repeats > 0) {
        parts.push_back(fmt::format("{} repeated", m_repeats));
    }
    if (auto file = file_path()) {
        parts.push_back(fmt::format("full log: {}", file->string()));
    }

    if (not msg.empty() or not parts.empty()) {
        fmt::println(stderr, "{}{}{}", msg, msg.empty() or parts.empty() ? "" : " ", fmt::join(parts, ", "));
    }

    m_errors = 0;
    m_warnings = 0;
    m_repeats = 0;
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

auto env_name(std::string_view name, std::string_view key) -> std::string {
    std::string result;
    for (char c: name) {
        result += std::isalnum(static_cast<unsigned char>(c)) != 0
            ? static_cast<char>(std::toupper(static_cast<unsigned char>(c)))
            : '_';
    }
    return fmt::format("{}_LOG_{}", result, key);
}

auto resolve(const std::string& name, const env_lookup& getenv) -> settings {
    settings s;
    s.file_dir = std::filesystem::temp_directory_path() / fmt::format("{}-logs", name);

    if (auto claude = getenv(EnvClaudeCode); claude.has_value() and not claude->empty()) {
        s.terminal_view = view::digest;
    }

    apply_legacy_mode(getenv, s);
    apply_env(getenv, EnvWindowSize, parse_positive, [&](std::size_t n) { s.window_size = n; });

    apply_env(getenv, env_name(name, "FORMAT"), parse_format, [&](format f) { s.terminal_format = f; });
    apply_env(getenv, env_name(name, "VIEW"), parse_view, [&](view v) { s.terminal_view = v; });
    apply_env(getenv, env_name(name, "FILE_FORMAT"), parse_format, [&](format f) { s.file_format = f; });
    apply_env(getenv, env_name(name, "WINDOW_SIZE"), parse_positive, [&](std::size_t n) { s.window_size = n; });
    apply_env(getenv, env_name(name, "LINE_MAX"), parse_positive, [&](std::size_t n) { s.line_max = n; });

    if (auto dir = getenv(env_name(name, "FILE")); dir.has_value()) {
        s.file_dir = dir->empty()
            ? std::nullopt
            : std::optional<std::filesystem::path>{ *dir };
    }

    return s;
}

void init(const std::string& name) {
    init(name, resolve(name, [](const std::string& var) -> std::optional<std::string> {
        if (auto value = nova::getenv(var); value.has_value()) {
            return *value;
        }
        return std::nullopt;
    }));
}

void init(const std::string& name, const settings& s) {
    nova::log::load_env_levels();
    spdlog::drop(name);

    std::shared_ptr<spdlog::logger> logger;
    switch (s.terminal_view) {
        case view::stream:
            logger = spdlog::create<spdlog::sinks::ansicolor_stderr_sink_mt>(name);
            break;
        case view::window:
            logger = spdlog::create<scrollog_sink>(name, name);
            if (auto sink = std::dynamic_pointer_cast<scrollog_sink>(logger->sinks().front())) {
                sink->scroll_window().visible_lines(s.window_size);
            }
            break;
        case view::digest:
            logger = spdlog::create<digest_sink>(name, s.line_max);
            break;
    }

    logger->set_pattern(pattern_of(s.terminal_format));
    spdlog::set_default_logger(logger);

    current() = state{ .name = name, .config = s, .file = std::nullopt };
}

void open_file(const std::string& label) {
    auto& st = current();
    if (not st.config.file_dir.has_value() or st.file.has_value()) {
        return;
    }

    // TODO(design): Nothing removes old files; `/tmp` is the only cleanup, a long-lived
    //               machine or a custom `<NAME>_LOG_FILE` grows without bound.
    const auto now = std::chrono::floor<std::chrono::milliseconds>(std::chrono::system_clock::now());
    const auto path = *st.config.file_dir / fmt::format("{:%Y%m%dT%H%M%SZ}_{}.log", now, label);

    std::shared_ptr<plain_file_sink> file_sink;
    try {
        file_sink = std::make_shared<plain_file_sink>(path);
    } catch (const spdlog::spdlog_ex& ex) {
        nova::log::warn("No log file: {}", ex.what());
        return;
    }
    file_sink->set_pattern(pattern_of(st.config.file_format));

    auto logger = spdlog::default_logger();
    const auto terminal_level = logger->level();
    const auto file_level = std::min(terminal_level, spdlog::level::info);

    for (auto& sink: logger->sinks()) {
        sink->set_level(terminal_level);
    }
    file_sink->set_level(file_level);
    logger->sinks().push_back(file_sink);
    logger->set_level(file_level);

    st.file = path;
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
        { EnvLogMode, "plain" },
        { env_name(name, "FORMAT"), "plain" },
        { env_name(name, "VIEW"), "stream" },
        { env_name(name, "FILE"), "" },
    };
}

void success(const std::string& msg, const std::string& logger_name) {
    if (auto window = find_sink<scrollog_sink>(logger_name)) {
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
    if (auto window = find_sink<scrollog_sink>(logger_name)) {
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
