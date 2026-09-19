/**
 * Part of Utility Library
 *
 * TTY utilities for building TUI applications.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 */

#pragma once

#include <fmt/format.h>

#include <concepts>
#include <cstddef>
#include <string>

namespace utl::tty {

/**
 * @brief   Clear the entire current terminal line.
 */
inline void clear_line() {
    fmt::print("\x1b[2K");
}

/**
 * @brief   Erase everything from the cursor to the end of the screen.
 *
 * Unlike `clear_line()`, this is unaffected by lines that wrap onto several
 * terminal rows: it wipes every row below the cursor in one go, so a
 * redraw never leaves stale characters behind from a previous, longer
 * (possibly multi-row) render.
 */
inline void erase_to_end() {
    fmt::print("\x1b[0J");
}

/**
 * @brief   Move the cursor up by `n` terminal rows.
 */
template <std::integral T = int>
inline void cursor_up(T n = 1) {
    fmt::print("\x1b[{}A", n);
}

/**
 * @brief   Move the cursor down by `n` terminal rows.
 */
template <std::integral T = int>
inline void cursor_down(T n = 1) {
    fmt::print("\x1b[{}B", n);
}

/**
 * @brief   Query the current terminal width in columns, falling back to
 *          80 when it cannot be determined (e.g. output is redirected).
 */
std::size_t terminal_width();

/**
 * @brief   Number of *visible* (i.e. non-ANSI-escape) characters in `line`.
 *
 * ANSI SGR escape sequences (e.g. color codes) occupy no columns on screen,
 * so they must be excluded before computing how many terminal columns a
 * line will actually occupy.
 */
std::size_t visible_width(const std::string& line);

/**
 * @brief   Truncate `line` to at most `max_width` *visible* characters,
 *          appending a `"..."` marker when it had to be shortened.
 *
 * Any trailing ANSI reset/color sequence already present in `line` is
 * preserved after the truncation point so color state isn't corrupted
 * mid-window; only visible characters count towards `max_width`.
 */
std::string cap_visible_width(const std::string& line, std::size_t max_width);

} // namespace utl::tty
