/**
 * Part of Utility Library
 *
 * @brief   TTY utilities for building TUI applications.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-07-12
 * @updated     2026-10-31
 */

#include <libutl/tty.hpp>

#include <string_view>

#include <sys/ioctl.h>
#include <unistd.h>

namespace utl::tty {

namespace detail {

/**
 * @brief   Advance `i` past a CSI (`\x1b[...`) escape sequence starting at
 *          `i`, returning the index of the sequence's final byte.
 *
 * Stops at the end of `line` if the sequence never terminates.
 */
std::size_t skip_csi_sequence(std::string_view line, std::size_t i) {
    i += 2;

    while (i < line.size() and (line[i] == ';' or (line[i] >= '0' and line[i] <= '9'))) {
        ++i;
    }

    if (i < line.size()) {
        ++i;
    }

    return i;
}

/**
 * @brief   Advance `i` past an OSC (`\x1b]...`) escape sequence starting at
 *          `i`, e.g. a hyperlink, terminated by BEL or ST (`\x1b\\`).
 *
 * Stops at the end of `line` if the sequence never terminates.
 */
std::size_t skip_osc_sequence(std::string_view line, std::size_t i) {
    for (i += 2; i < line.size(); ++i) {
        if (line[i] == '\x07') {
            return i + 1;
        }

        if (line[i] == '\x1b' and i + 1 < line.size() and line[i + 1] == '\\') {
            return i + 2;
        }
    }

    return line.size();
}

} // namespace detail

std::size_t terminal_width() {
    struct winsize ws {};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 and ws.ws_col > 0) {
        return static_cast<std::size_t>(ws.ws_col);
    }
    return 80;
}

std::size_t visible_width(const std::string& line) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < line.size();) {
        if (line[i] == '\x1b' and i + 1 < line.size() and line[i + 1] == '[') {
            i = detail::skip_csi_sequence(line, i);
            continue;
        }
        ++width;
        ++i;
    }
    return width;
}

std::string cap_visible_width(const std::string& line, std::size_t max_width) {
    if (visible_width(line) <= max_width) {
        return line;
    }

    static constexpr std::string_view suffix = "...";
    std::size_t budget = max_width > suffix.size() ? max_width - suffix.size() : 0;

    std::string result;
    std::string trailing_escape;
    std::size_t visible = 0;
    for (std::size_t i = 0; i < line.size();) {
        if (line[i] == '\x1b' and i + 1 < line.size() and line[i + 1] == '[') {
            std::size_t j = detail::skip_csi_sequence(line, i);
            std::string escape = line.substr(i, j - i);
            if (visible >= budget) {
                trailing_escape += escape;
            } else {
                result += escape;
            }
            i = j;
            continue;
        }

        if (visible < budget) {
            result += line[i];
            ++visible;
        }
        ++i;
    }

    result += suffix;
    result += trailing_escape;
    return result;
}

std::string strip_ansi(std::string_view line) {
    std::string result;
    result.reserve(line.size());

    for (std::size_t i = 0; i < line.size();) {
        if (line[i] == '\x1b' and i + 1 < line.size() and line[i + 1] == '[') {
            i = detail::skip_csi_sequence(line, i);
        } else if (line[i] == '\x1b' and i + 1 < line.size() and line[i + 1] == ']') {
            i = detail::skip_osc_sequence(line, i);
        } else {
            result += line[i];
            ++i;
        }
    }

    return result;
}

} // namespace utl::tty
