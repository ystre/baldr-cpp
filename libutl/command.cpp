#include <libutl/command.hpp>

#include <fmt/format.h>

#include <cerrno>
#include <cstring>
#include <tuple>

#include <csignal>
#include <cstdlib>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>


namespace utl {

pty::pty() {
    m_master = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (m_master == -1) {
        throw nova::exception("Failed to open pty master");
    }

    if (::grantpt(m_master) == -1 || ::unlockpt(m_master) == -1) {
        ::close(m_master);
        throw nova::exception("Failed to configure pty");
    }

    std::array<char, 64> path{};
    if (::ptsname_r(m_master, path.data(), path.size()) != 0) {
        ::close(m_master);
        throw nova::exception("Failed to resolve pty slave path");
    }

    m_slave_path = path.data();

    winsize ws{};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0 || ws.ws_row == 0) {
        ws.ws_row = 50;
        ws.ws_col = 200;
    }
    ::ioctl(m_master, TIOCSWINSZ, &ws);
}

void pty::redirect_child(file_descriptor fd) const {
    int slave = ::open(m_slave_path.c_str(), O_RDWR);
    if (slave == -1) {
        throw nova::exception("Failed to open pty slave");
    }

    switch (fd) {
        case file_descriptor::stdout:
            dup2(slave, STDOUT_FILENO);
            break;
        case file_descriptor::stderr:
            dup2(slave, STDERR_FILENO);
            break;
        case file_descriptor::both:
            dup2(slave, STDOUT_FILENO);
            dup2(slave, STDERR_FILENO);
            break;
    }

    if (slave > STDERR_FILENO) {
        ::close(slave);
    }
    ::close(m_master);
}


[[nodiscard]] auto command::exit_status::describe() const -> std::string {
    switch (m_type) {
        case kind::exited:
            return fmt::format("`{}` exited with code {}.", m_name, m_value);
        case kind::signaled:
            return fmt::format("`{}` was terminated by signal {} ({}).", m_name, m_value, strsignal(m_value));
        case kind::exec_failed:
            return fmt::format("Failed to execute `{}`: {}.", m_name, strerror(m_value));
    }
    return fmt::format("`{}` exited abnormally.", m_name);
}

void command::pipe::redirect(file_descriptor fd) const {
    switch (fd) {
        case file_descriptor::stdout:
            redirect_impl(STDOUT_FILENO);
            break;
        case file_descriptor::stderr:
            redirect_impl(STDERR_FILENO);
            break;
        case file_descriptor::both:
            redirect_impl(STDOUT_FILENO);
            redirect_impl(STDERR_FILENO);
            break;
    }
    close_write();
}

void command::pipe::redirect_impl(int fd) const {
    if (dup2(write(), fd) == -1) {
        throw nova::exception("Pipe duplication failed");
    };
}

command::command(
        const std::vector<std::string>& args,
        const std::map<std::string, std::string>& env,
        std::string working_directory,
        bool interactive
)
    : m_args_vec(args)
    , m_env_map(env)
    , m_interactive(interactive)
    , m_working_directory(std::move(working_directory))
{
    m_args.reserve(m_args_vec.size() + 1);
    for (const auto& arg : m_args_vec) {
        m_args.push_back(const_cast<char*>(arg.c_str()));
    }
    m_args.push_back(nullptr);
}

void command::run() {
    if (fcntl(m_error_pipe.write(), F_SETFD, FD_CLOEXEC) == -1) {
        throw nova::exception("Failed to configure error pipe");
    }

    m_pid = fork();
    if (m_pid == -1) {
        throw nova::exception("Failed to fork process");
    }

    if (m_pid == 0) {
        m_error_pipe.close_read();

        if (not m_interactive) {
            if (setsid() == -1) {
                int setsid_errno = errno;
                std::ignore = ::write(m_error_pipe.write(), &setsid_errno, sizeof(setsid_errno));
                _exit(EXIT_FAILURE);
            }
            m_pty.redirect_child(file_descriptor::both);
        }

        for (const auto& [key, value] : m_env_map) {
            setenv(key.c_str(), value.c_str(), 1);
        }

        if (not m_working_directory.empty() && chdir(m_working_directory.c_str()) == -1) {
            int chdir_errno = errno;
            std::ignore = ::write(m_error_pipe.write(), &chdir_errno, sizeof(chdir_errno));
            _exit(EXIT_FAILURE);
        }

        execvp(m_args[0], m_args.data());
        int exec_errno = errno;
        std::ignore = ::write(m_error_pipe.write(), &exec_errno, sizeof(exec_errno));
        _exit(EXIT_FAILURE);
    }

    m_error_pipe.close_write();
    int exec_errno = 0;
    ssize_t n = ::read(m_error_pipe.read(), &exec_errno, sizeof(exec_errno));
    m_error_pipe.close_read();
    if (n == sizeof(exec_errno)) {
        m_exec_failed = true;
        m_exec_errno = exec_errno;
    }
}

auto command::poll() -> std::string {
    if (m_interactive) {
        return {};
    }

    ssize_t n = 0;
    do {
        n = ::read(m_pty.master(), m_buffer.data(), m_buffer.size());
    } while (n == -1 && errno == EINTR);

    if (n <= 0) {
        return {};
    }

    return {
        m_buffer.data(),
        static_cast<std::size_t>(n)
    };
}

auto command::wait() -> exit_status {
    if (not m_interactive) {
        ::close(m_pty.master());
    }

    int status = 0;
    struct rusage raw_usage {};
    while (wait4(m_pid, &status, 0, &raw_usage) == -1 && errno == EINTR) {
        // Retry rather than trust a `status` a failed wait4() never
        // wrote to; see Developer's Manual for a detailed explanation ("SA_RESTART").
    }

    if (m_exec_failed) {
        return { m_args[0], exit_status::kind::exec_failed, m_exec_errno };
    }

    auto usage = create_resource_usage(raw_usage);

    if (WIFSIGNALED(status)) {
        return { m_args[0], exit_status::kind::signaled, WTERMSIG(status), usage };
    }

    if (WIFEXITED(status)) {
        return { m_args[0], exit_status::kind::exited, WEXITSTATUS(status), usage };
    }

    return { m_args[0], exit_status::kind::exited, EXIT_FAILURE, usage };
}

} // namespace utl
