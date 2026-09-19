#include <libutl/tty.hpp>

#include <string_view>

#include <sys/ioctl.h>
#include <unistd.h>

namespace utl::tty {

namespace detail {

/**
 * @brief   Advance `i` past a CSI (`\x1b[...`) escape sequence starting at
 *          `i`, returning the index of the sequence's final byte, exclusive
 *          (i.e. one past the terminating byte).
 *
 * Assumes `line[i] == '\x1b'` and `line[i + 1] == '['`. Stops gracefully at
 * the end of `line` if the sequence is truncated/malformed and never
 * terminates.
 */
std::size_t skip_csi_sequence(const std::string& line, std::size_t i) {
    std::size_t j = i + 2;
    while (j < line.size() and (line[j] == ';' or (line[j] >= '0' and line[j] <= '9'))) {
        ++j;
    }
    if (j < line.size()) {
        ++j;
    }
    return j;
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

} // namespace utl::tty
