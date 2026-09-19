/**
 * Part of Utility Library
 *
 * RAII signal-handler installer (`signal_guard`) and a reusable
 * count-then-escalate handler policy (`signal_handler`) that can be
 * installed through it.
 *
 * @author      Gábor Krisztián Girhiny
 * @coauthor    Claude Sonnet 5 (Claude)
 * @date        2026-08-06
 */

#pragma once

#include <libnova/error.hpp>

#include <array>

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <sys/types.h>

namespace utl {

/**
 * @brief   RAII installer for a signal handler, with no opinion on what the
 *          handler does.
 *
 * Swaps a signal's disposition for `handler` and restores whatever was
 * there before once the guard goes out of scope. `restart` maps directly to
 * `SA_RESTART`; `SA_RESETHAND` is deliberately not offered. Each signal
 * number has its own installed/not-installed bookkeeping — installing a
 * second guard for the *same* signal number while the first is still alive
 * throws rather than silently stomping the first guard's bookkeeping.
 *
 * `handler` itself must be async-signal-safe (see `signal-safety(7)`): no
 * allocation, no locks, no exceptions, no non-reentrant libc calls. See
 * `signal_handler` for a ready-made handler that stays inside those bounds.
 */
class signal_guard {
public:
    using handler_fn = void (*)(int);

    explicit signal_guard(int signal_number, handler_fn handler, bool restart = true)
        : m_signal_number(signal_number)
    {
        if (s_installed[static_cast<std::size_t>(m_signal_number)]) {
            throw nova::exception("A signal_guard for signal {} is already installed.", m_signal_number);
        }

        struct sigaction action {};
        action.sa_handler = handler;
        sigemptyset(&action.sa_mask);
        action.sa_flags = restart ? SA_RESTART : 0;

        if (sigaction(m_signal_number, &action, &m_previous) == -1) {
            throw nova::exception("Failed to install handler for signal {}: {}", m_signal_number, strerror(errno));
        }

        s_installed[static_cast<std::size_t>(m_signal_number)] = true;
    }

    /**
     * @brief   Restores the previous signal handler and clears the slot.
     */
    ~signal_guard() {
        sigaction(m_signal_number, &m_previous, nullptr);
        s_installed[static_cast<std::size_t>(m_signal_number)] = false;
    }

    signal_guard(const signal_guard&) = delete;
    signal_guard& operator=(const signal_guard&) = delete;
    signal_guard(signal_guard&&) = delete;
    signal_guard& operator=(signal_guard&&) = delete;

    /**
     * @brief   Restore the signal to its default (terminating) disposition
     *          and re-raise it against this process, so an outer
     *          shell/script observes the conventional signal-terminated
     *          exit status (e.g. 130 for `SIGINT`) instead of whatever this
     *          process would otherwise `return` from `main`.
     *
     * Never returns under normal circumstances, since the default
     * disposition terminates the process; `_exit()` is a fallback for the
     * pathological case where the signal is somehow blocked.
     */
    [[noreturn]] void reraise_default() const {
        struct sigaction dfl {};
        dfl.sa_handler = SIG_DFL;
        sigemptyset(&dfl.sa_mask);
        sigaction(m_signal_number, &dfl, nullptr);
        raise(m_signal_number);
        _exit(128 + m_signal_number);
    }

private:
    int m_signal_number;
    struct sigaction m_previous {};

    static inline std::array<bool, NSIG> s_installed{};
};

/**
 * @brief   A closed set of actions safe to fire from inside a signal
 *          handler
 *
 * Deliberately not an arbitrary callback ensuring signal safety.
 *
 */
enum class escalation_action : std::uint8_t {
    none,       ///< No escalation configured / disarmed.
    kill_pid,   ///< `kill(target, SIGKILL)` — take down a watched child.
    abort_self, ///< `abort()` — take *this* process down (e.g. reused inside a child).
};

/**
 * @brief   A reusable "count deliveries, escalate after N" signal-handler
 *          policy, installable via `signal_guard`
 *          (`signal_guard{ signum, &signal_handler::handle }`).
 *
 * All state is static and keyed by signal number, not tied to a
 * `signal_guard` instance — if nothing ever installed `handle` for a given
 * signal, calls like `watch()`/`triggered()` are simply inert.
 */
class signal_handler {
public:
    /**
     * @brief   Default number of deliveries after which a `watch()`-ed
     *          child is sent `SIGKILL`.
     */
    static constexpr auto ForceKillAfter = 3;

    /**
     * @brief   The handler function to install via `signal_guard`, e.g.
     *          `signal_guard{ SIGINT, &signal_handler::handle }`.
     *
     * Async-signal-safe: only touches `volatile` scalars and, once the
     * configured threshold is reached, calls `kill()`/`abort()`.
     */
    static void handle(int signum) {
        auto idx = static_cast<std::size_t>(signum);
        auto count = s_flags[idx] + 1;
        s_flags[idx] = count;

        if (auto after = s_after[idx]; after > 0 && count >= after) {
            dispatch(s_action[idx], s_target[idx]);
        }
    }

    /**
     * @brief   Number of times `signum` has been delivered since the last
     *          `reset()` (or since the process started).
     */
    [[nodiscard]] static auto triggered(int signum) -> int {
        return s_flags[static_cast<std::size_t>(signum)];
    }

    /**
     * @brief   Clear the delivery count for `signum`.
     */
    static void reset(int signum) {
        s_flags[static_cast<std::size_t>(signum)] = 0;
    }

    /**
     * @brief   Arm an escalation for `signum`: once it has been delivered
     *          `after` times, `action` fires against `target` (meaningful
     *          only for actions that need one, e.g. `kill_pid`).
     */
    static void escalate(int signum, escalation_action action, int after, pid_t target = -1) {
        auto idx = static_cast<std::size_t>(signum);
        s_target[idx] = target;
        s_after[idx] = after;
        s_action[idx] = action;
    }

    /**
     * @brief   Disarm whatever escalation was configured for `signum`.
     */
    static void disarm(int signum) {
        auto idx = static_cast<std::size_t>(signum);
        s_action[idx] = escalation_action::none;
        s_after[idx] = 0;
        s_target[idx] = -1;
    }

    /**
     * @brief   Sugar for `escalate(signum, kill_pid, force_kill_after, pid)`
     *          — register `pid` as the child to `SIGKILL` once `signum` has
     *          arrived `force_kill_after` times, so a child that's
     *          ignoring/outliving the signal still gets taken down instead
     *          of being abandoned as an orphan.
     *
     * Prefer `scoped_watch` over calling this directly.
     */
    static void watch(int signum, pid_t pid, int force_kill_after = ForceKillAfter) {
        escalate(signum, escalation_action::kill_pid, force_kill_after, pid);
    }

    /**
     * @brief   Sugar for `disarm(signum)`, e.g. because the watched child
     *          has already been reaped.
     */
    static void unwatch(int signum) {
        disarm(signum);
    }

    /**
     * @brief   RAII wrapper around `watch()`/`unwatch()`, so an exception
     *          escaping the watched scope (e.g. from output processing
     *          between spawning a child and `wait()`-ing for it) can't
     *          leave a stale `pid` registered for the `SIGKILL` escalation.
     */
    class scoped_watch {
    public:
        scoped_watch(int signum, pid_t pid, int force_kill_after = ForceKillAfter)
            : m_signum(signum)
        {
            signal_handler::watch(m_signum, pid, force_kill_after);
        }

        ~scoped_watch() {
            signal_handler::unwatch(m_signum);
        }

        scoped_watch(const scoped_watch&) = delete;
        scoped_watch& operator=(const scoped_watch&) = delete;
        scoped_watch(scoped_watch&&) = delete;
        scoped_watch& operator=(scoped_watch&&) = delete;

    private:
        int m_signum;
    };

private:
    static inline std::array<volatile sig_atomic_t, NSIG> s_flags{};
    static inline std::array<volatile pid_t, NSIG> s_target{};
    static inline std::array<volatile int, NSIG> s_after{};
    static inline std::array<volatile escalation_action, NSIG> s_action{};

    /**
     * @brief   Dispatches the closed action set; kept separate from
     *          `handle()` so a new action never touches the counting logic.
     */
    static void dispatch(escalation_action action, pid_t target) {
        switch (action) {
            case escalation_action::none:
                break;
            case escalation_action::kill_pid:
                if (target > 0) {
                    kill(target, SIGKILL);
                }
                break;
            case escalation_action::abort_self:
                abort();
                break;
        }
    }
};

} // namespace utl
