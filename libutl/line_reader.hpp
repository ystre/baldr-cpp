/**
 * Part of Utility Library
 *
 * Scrollog — a scrolling, rolling-window log/progress renderer.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 */
#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace utl {

/**
 * @brief   Reassembles complete, newline-delimited lines from raw chunks
 *          fed incrementally, dispatching each to a stored callback.
 *
 * This is a reusable, opt-in layer that sits on top of any raw, chunk-based
 * reader (such as a process' output pipe), which itself makes no assumption
 * about the byte stream being text.
 */
template <typename Callable>
    requires std::invocable<Callable, std::string&>
class line_reader {
public:
    explicit line_reader(Callable callback)
        : m_callback(std::move(callback))
    {}

    /**
     * @brief   Feed a raw chunk into the reader, dispatching the callback
     *          once per complete line found within the accumulated buffer.
     */
    void feed(std::string_view chunk) {
        m_buf.append(chunk);

        std::size_t pos;
        while ((pos = m_buf.find('\n')) != std::string::npos) {
            std::string line = m_buf.substr(0, pos);
            std::invoke(m_callback, line);
            m_buf.erase(0, pos + 1);
        }
    }

    /**
     * @brief   Flush any trailing partial line still buffered (no trailing
     *          newline) exactly once. Subsequent calls are no-ops.
     */
    void feed_eof() {
        if (m_buf.empty()) {
            return;
        }

        std::invoke(m_callback, m_buf);
        m_buf.clear();
    }

private:
    Callable m_callback;
    std::string m_buf;
};

} // namespace utl
