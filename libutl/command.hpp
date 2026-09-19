/**
 * Part of Utility Library
 *
 * Process spawning.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Junie)
 * @date        2026-07-12
 */

#pragma once

#include <array>
#include <string>
#include <vector>
#include <map>

#include <libnova/error.hpp>

#include <chrono>
#include <csignal>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>

namespace utl {

struct resource_usage {
    std::chrono::microseconds user_time { 0 };
    std::chrono::microseconds system_time { 0 };
    long peak_rss_kb = 0;   // `ru_maxrss`; kilobytes on Linux.
};

[[nodiscard]] inline
auto create_resource_usage(const rusage raw_usage) -> resource_usage {
    return {
        .user_time   = std::chrono::seconds{raw_usage.ru_utime.tv_sec} + std::chrono::microseconds{raw_usage.ru_utime.tv_usec},
        .system_time = std::chrono::seconds{raw_usage.ru_stime.tv_sec} + std::chrono::microseconds{raw_usage.ru_stime.tv_usec},
        .peak_rss_kb = raw_usage.ru_maxrss,
    };
}

enum class file_descriptor {
    stdout,
    stderr,
    both
};

/**
 * @brief   A wrapper around a pseudo-terminal, used in place of a
 *          plain `pipe(2)` for the child's stdout/stderr.
 *
 * Unlike a pipe, a pty makes the child (and anything it in turn spawns) see
 * a real terminal on `isatty()`, so they keep emitting their own ANSI
 * colors instead of silently falling back to plain text because they think
 * they're talking to a file/pipe.
 */
class pty {
public:
    pty();

    [[nodiscard]] auto master() const -> int { return m_master; }

    /**
     * @brief   Open the slave side and dup it onto `fd`, then close both
     *          the parent's master and the temporary slave descriptor.
     *
     * Child-side only: called after `fork()`, before `execvp()`.
     */
    void redirect_child(file_descriptor fd) const;

private:
    int m_master = -1;
    std::string m_slave_path;

};

/**
 * @brief   Spawns a child process, exposing its combined stdout/stderr
 *          output through a pull-based `poll()`, or leaving the child
 *          attached to the parent's own TTY in `interactive` mode.
 *
 * Environment variable overrides (`m_env_map`) are added on top of the
 * current process's environment, e.g. to force `CC`/`CXX` for a `cmake`
 * invocation.
 */
class command {
public:

    /**
     * @brief   Detailed outcome of a finished process, distinguishing a
     *          normal exit from being killed by a signal or from `execvp`
     *          itself failing (e.g. the executable doesn't exist).
     */
    class exit_status {
    public:
        enum class kind {
            exited,       // Process ran and exited normally; `value` is the exit code.
            signaled,     // Process was killed by a signal; `value` is the signal number.
            exec_failed,  // `execvp` itself failed; `value` is the `errno` from `execvp`.
        };

        exit_status(const char* name, kind type, int value, resource_usage usage = {})
            : m_name(name)
            , m_type(type)
            , m_value(value)
            , m_usage(usage)
        {}

        [[nodiscard]] auto success() const -> bool {
            return m_type == kind::exited
                && m_value == 0;
        }

        /**
         * @brief   Shell-style numeric exit code, for callers that only
         *          care about a single integer (128+signal for signals,
         *          127 for a failed `execvp`, following common convention).
         */
        [[nodiscard]] auto code() const -> int {
            switch (m_type) {
                case kind::exited:      return m_value;
                case kind::signaled:    return 128 + m_value;
                case kind::exec_failed: return 127;
            }
            return EXIT_FAILURE;
        }

        /**
         * @brief   Whether the process was terminated by `SIGINT`, i.e. the
         *          user hit Ctrl-C rather than the process actually failing.
         */
        [[nodiscard]] auto interrupted() const -> bool {
            return m_type == kind::signaled
                && m_value == SIGINT;
        }

        /**
         * @brief   Human-readable description of the outcome, naming the signal
         *          or the `errno` reason instead of just a bare, ambiguous exit code.
         */
        [[nodiscard]] auto describe() const -> std::string;

        /**
         * @brief   Resource usage accumulated by the process.
         */
        [[nodiscard]] auto usage() const -> const resource_usage& {
            return m_usage;
        }

    private:
        std::string m_name;
        kind m_type = kind::exited;
        int m_value = 0;
        resource_usage m_usage;

    };

    /**
     * @brief   RAII wrapper around a `pipe(2)` pair, with helpers to
     *          redirect the write end onto a standard file descriptor in
     *          the child process.
     */
    class pipe {
    public:
        pipe() {
            if (::pipe(m_inner.data()) == -1) {
                throw nova::exception("Pipe creation failed");
            };
        }

        [[nodiscard]] auto read()  const -> int { return m_inner[0]; }
        [[nodiscard]] auto write() const -> int { return m_inner[1]; }

        void close_read() const {
            ::close(read());
        }

        void close_write() const {
            ::close(write());
        }

        void redirect(file_descriptor fd) const;

    private:
        std::array<int, 2> m_inner {};

        void redirect_impl(int fd) const;
    };

    command(
        const std::vector<std::string>& args,
        const std::map<std::string, std::string>& env = {},
        std::string working_directory = {},
        bool interactive = false
    );

    /**
     * @brief   Fork and exec the process. In non-interactive mode, the
     *          child's stdout/stderr are redirected onto a pty, so the
     *          child (and anything it spawns) still thinks it's talking to
     *          a real terminal and keeps emitting its own colors.
     */
    void run();

    /**
     * @brief   Read and return the next available chunk of combined
     *          stdout/stderr output.
     *
     * @return  The chunk read, or an empty string on EOF/`interactive` mode.
     *          An empty return does not necessarily mean EOF for a single
     *          `read()`; callers should keep polling until `wait()`.
     *
     * A pty master reports the child side going away as `read()` failing
     * with `EIO` rather than returning `0`, unlike a plain pipe; both are
     * folded into the same empty-return, end-of-output case here.
     */
    auto poll() -> std::string;

    /**
     * @brief   PID of the spawned process, valid once `run()` has returned.
     *
     * Useful for a caller that wants to act on the child independently of
     * this class, e.g. registering it with `signal_handler::watch()` for a
     * force-kill escalation while `poll()`/`wait()` are in progress.
     */
    [[nodiscard]] auto pid() const -> pid_t {
        return m_pid;
    }

    /**
     * @brief   Wait for the process to exit.
     *
     * @return  The detailed outcome, distinguishing a normal exit from a
     *          signal or a failed `execvp`, and carrying its resource usage
     *          (CPU time, peak memory) as reported by `wait4(2)`.
     */
    auto wait() -> exit_status;

private:
    std::vector<std::string> m_args_vec;
    std::vector<char*> m_args;
    std::map<std::string, std::string> m_env_map;
    pty m_pty;
    pipe m_error_pipe;
    pid_t m_pid = -1;
    bool m_interactive = false;
    std::string m_working_directory;
    bool m_exec_failed = false;
    int m_exec_errno = 0;

    static constexpr auto BufferSize = 4096;
    std::array<char, BufferSize> m_buffer{ };
};

} // namespace utl
