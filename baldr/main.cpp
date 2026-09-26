/**
 * Part of Baldr.
 *
 * Baldr CLI entry point.
 *
 * @author      Gábor Krisztián Girhiny
 * @date        2026-07-12
 */

#include <baldr/builder.hpp>
#include <baldr/config.hpp>
#include <baldr/docker.hpp>

#include <libutl/rlog.hpp>
#include <libutl/signal.hpp>

#include <libnova/error.hpp>
#include <libnova/log.hpp>
#include <libnova/main.hpp>

#include <boost/program_options.hpp>

#include <fmt/format.h>

#include <unistd.h>

#include <algorithm>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace po = boost::program_options;

namespace {

#ifndef BALDR_VERSION
#define BALDR_VERSION "unknown"
#endif

#ifndef BALDR_GIT_HASH
#define BALDR_GIT_HASH "unknown"
#endif

#ifndef BALDR_GIT_BRANCH
#define BALDR_GIT_BRANCH "unknown"
#endif

#ifndef BALDR_GIT_DIRTY
#define BALDR_GIT_DIRTY "unknown"
#endif

[[nodiscard]] auto baldr_version() {
    const std::string dirty_suffix = std::string{BALDR_GIT_DIRTY} == "dirty" ? ".dirty" : "";
#ifdef BALDR_STATIC_LINK
    constexpr std::string_view linkage = "static";
#else
    constexpr std::string_view linkage = "dynamic";
#endif
    return fmt::format(
        "baldr v{}+{}.{}{} ({})",
        BALDR_VERSION,
        BALDR_GIT_HASH,
        BALDR_GIT_BRANCH,
        dirty_suffix,
        linkage
    );
}

/**
 * @brief   Build the description of Baldr's *visible* options, i.e.
 *          everything except the positional `command`/`args` (which are
 *          kept in a separate, hidden description so they don't show up as
 *          fake `--command`/`--args` flags in `print_help()`'s output).
 */
[[nodiscard]] auto build_options_description() -> po::options_description {
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Produce help message")
        ("version,v", "Print version and exit")
        ("project,p", po::value<std::string>()->default_value("."), "Project directory")
        ("build-type,b", po::value<std::string>()->default_value("Debug"), "CMake build type/output subdir")
        ("build-dir", po::value<std::string>(), "Override the default build directory (default: build/<build_type>)")
        ("clean", po::bool_switch()->default_value(false), "For 'build': wipe the build directory before building (clean build)")
        ("cmake-define,D", po::value<std::vector<std::string>>()->composing(), "CMake define KEY=VALUE, repeatable; triggers reconfigure on change")
        ("target,t", po::value<std::string>(), "Executable name to run (for 'run'; mutually exclusive with -x/--exec)")
        ("exec,x", po::value<std::string>(), "Arbitrary executable (script or binary) to run instead of a built target (for 'run')")
        ("build", po::bool_switch()->default_value(false), "For 'run': build the project first (not with -x/--exec)")
        ("debug", po::bool_switch()->default_value(false), "For 'run': launch the target under the configured debugger (default: 'gdb --args')")
        ("image,i", po::value<std::string>(), "Docker image to use (required for 'docker'; for 'build'/'run', re-executes inside a container of this image)")
    ;
    return desc;
}

/**
 * @brief   Print the usage/help message to `out`, rendering `desc`
 *          (see `build_options_description()`) for the flag table.
 */
void print_help(std::ostream& out, const po::options_description& desc) {
    out << "Usage: baldr [options] <command>\n";
    out << desc << '\n';
    out << "      -- <args...>          For 'run': forward everything after '--' to the target's own argv\n";
    out << "\n";
    out << "  CMake projects are always configured with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON;\n";
    out << "  for the 'Debug' build type, <project_dir>/compile_commands.json is kept\n";
    out << "  symlinked to it (no need to switch it for other build types).\n";
    out << "\n";
    out << "  A project-local '.baldr.yaml' (falling back to '~/.baldr.yaml') can supply\n";
    out << "  default 'build_type', 'cmake.definitions' and 'cmake.env'; CLI flags always\n";
    out << "  take precedence over config values.\n";
    out << "\n";
    out << "  '-i/--image <image>' on 'build'/'run' re-executes the equivalent baldr\n";
    out << "  invocation inside a fresh container of <image>: the project directory is\n";
    out << "  bind-mounted to /workspace, baldr's own binary is bind-mounted in (unless\n";
    out << "  'docker.mount-baldr: false' in .baldr.yaml), and the container process runs\n";
    out << "  as the host's uid:gid. The target image must already have the project's\n";
    out << "  toolchain installed.\n";
    out << "\n";
    out << "Commands:\n";
    out << "  build      Configure (if needed) and build the project\n";
    out << "  run        Run a built target (-t/--target) or an arbitrary executable (-x/--exec)\n";
}

/**
 * @brief   Resolve the path of the currently running `baldr` binary, for
 *          bind-mounting it into a `--image` container.
 */
[[nodiscard]] auto self_exe_path() -> std::filesystem::path {
    return std::filesystem::read_symlink("/proc/self/exe");
}

enum class command_type {
    build,
    run,
};

/**
 * @brief   Outcome of parsing the CLI arguments.
 *
 * Returned wrapped in `std::optional` by `parse_args()`: `std::nullopt`
 * means the process should exit successfully without running any command
 * (e.g. `--help`/`--version` was given, and already handled); otherwise
 * every field below is meaningful.
 */
struct options {
    command_type command;
    std::string project_dir;
    std::string build_type;
    std::optional<std::string> build_dir;
    bool clean_build = false;
    std::map<std::string, std::string> cmake_defines;
    bool build_type_explicit = false;
    std::optional<std::string> target;
    std::optional<std::string> exec;
    bool debug = false;
    std::optional<std::string> image;
    std::vector<std::string> docker_args;
    std::vector<std::string> forwarded_args;
    std::optional<std::string> stage;
    std::optional<std::string> scenario;
    std::map<std::string, std::string> params;
};

/**
 * @brief   Parse `args` (excluding the program name) into a `options`.
 *
 * Everything following the literal `--` is forwarded verbatim to the target's
 * own argv (for 'run') and never handed to boost::program_options itself.
 *
 * @param   args    Command line arguments, excluding `argv[0]`.
 *
 * @return  Parsed options, or `std::nullopt` if `--help`/`--version` was
 *          given (already printed to stdout).
 *
 * @throws  nova::exception on an invalid combination of flags/command.
 */
[[nodiscard]] auto parse_args(const std::vector<std::string>& all_args) -> std::optional<options> {
    std::vector<std::string> args;
    std::vector<std::string> forwarded_args;
    if (auto it = std::ranges::find(all_args, std::string("--")); it != all_args.end()) {
        args.assign(all_args.begin(), it);
        forwarded_args.assign(std::next(it), all_args.end());
    } else {
        args = all_args;
    }

    auto desc = build_options_description();

    // Kept out of `desc` (and so out of `print_help()`'s output) since these
    // are positional-only, not real `--command`/`--args` flags.
    po::options_description positional_desc("Positional arguments");
    positional_desc.add_options()
        ("command", po::value<std::string>(), "Command to run: 'build', 'run'")
        ("args", po::value<std::vector<std::string>>()->multitoken(), "For 'docker': the container command")
    ;

    po::options_description all_options;
    all_options.add(desc).add(positional_desc);

    po::positional_options_description positional;
    positional.add("command", 1);
    positional.add("args", -1);

    po::variables_map vm;
    po::store(po::command_line_parser(args).options(all_options).positional(positional).run(), vm);

    if (vm.contains("help")) {
        print_help(std::cout, desc);
        return std::nullopt;
    }

    po::notify(vm);

    if (vm.contains("version")) {
        std::cout << baldr_version() << '\n';
        return std::nullopt;
    }

    options result;
    result.project_dir = vm["project"].as<std::string>();
    result.build_type = vm["build-type"].as<std::string>();
    result.build_type_explicit = not vm["build-type"].defaulted();
    result.clean_build = vm["clean"].as<bool>();
    if (vm.contains("build-dir")) {
        result.build_dir = vm["build-dir"].as<std::string>();
    }
    result.debug = vm["debug"].as<bool>();
    result.forwarded_args = std::move(forwarded_args);

    if (vm.contains("cmake-define")) {
        for (const auto& define: vm["cmake-define"].as<std::vector<std::string>>()) {
            auto eq_pos = define.find('=');
            if (eq_pos == std::string::npos) {
                throw nova::exception("Invalid -D/--define value '{}' (expected KEY=VALUE)", define);
            }
            result.cmake_defines[define.substr(0, eq_pos)] = define.substr(eq_pos + 1);
        }
    }

    if (vm.contains("target")) {
        result.target = vm["target"].as<std::string>();
    }

    if (vm.contains("exec")) {
        result.exec = vm["exec"].as<std::string>();
    }

    if (vm.contains("image")) {
        result.image = vm["image"].as<std::string>();
    }

    if (vm.contains("args")) {
        result.docker_args = vm["args"].as<std::vector<std::string>>();
    }

    if (vm.contains("stage")) {
        result.stage = vm["stage"].as<std::string>();
    }

    if (vm.contains("scenario")) {
        result.scenario = vm["scenario"].as<std::string>();
    }

    if (vm.contains("param")) {
        for (const auto& param: vm["param"].as<std::vector<std::string>>()) {
            auto eq_pos = param.find('=');
            if (eq_pos == std::string::npos) {
                throw nova::exception("Invalid -P/--param value '{}' (expected KEY=VALUE)", param);
            }
            result.params[param.substr(0, eq_pos)] = param.substr(eq_pos + 1);
        }
    }

    if (not vm.contains("command")) {
        print_help(std::cerr, desc);
        throw nova::exception("No command given");
    }

    if (auto cmd = vm["command"].as<std::string>(); cmd == "build") {
        result.command = command_type::build;
    } else if (cmd == "run") {
        result.command = command_type::run;
        if (not result.target and not result.exec) {
            throw nova::exception("'run' requires -t/--target <name> or -x/--exec <path>");
        }
        if (result.target and result.exec) {
            throw nova::exception("'run' accepts either -t/--target or -x/--exec, not both");
        }
    } else {
        throw nova::exception("Unknown command: {}", cmd);
    }

    if (result.debug and result.command != command_type::run) {
        throw nova::exception("'--debug' only applies to 'run'");
    }

    return result;
}

/**
 * @brief   Build the argv for the container-side `baldr`.
 *
 * Re-exec of `opts` (a `build`/`run` invocation), replaying every flag that
 * affects the container-local build/run except `-i/--image` itself and
 * `-p/--project` (rewritten to the bind-mounted `/workspace`).
 *
 * TODO(refact): Reflection to "serialize" `options` into arguments.
 *
 * @param   baldr_path  Container-side path to invoke (the bind-mounted
 *                      binary's mount point, or a bare name if the image is
 *                      expected to already provide its own `baldr`).
 */
[[nodiscard]] auto build_container_argv(const options& opts, const std::string& baldr_path) -> std::vector<std::string> {
    std::vector<std::string> argv{
        baldr_path,
        opts.command == command_type::build ? "build" : "run",
        "-p", "/workspace"
    };

    if (opts.build_type_explicit) {
        argv.emplace_back("-b");
        argv.push_back(opts.build_type);
    }
    if (opts.build_dir) {
        argv.emplace_back("--build-dir");
        argv.push_back(*opts.build_dir);
    }
    if (opts.clean_build) {
        argv.emplace_back("--clean");
    }
    for (const auto& [key, value] : opts.cmake_defines) {
        argv.emplace_back("-D");
        argv.push_back(fmt::format("{}={}", key, value));
    }

    if (opts.target) {
        argv.emplace_back("-t");
        argv.push_back(*opts.target);
    } else if (opts.exec) {
        argv.emplace_back("-x");
        argv.push_back(*opts.exec);
    }

    if (opts.command == command_type::run) {
        if (opts.debug) {
            argv.emplace_back("--debug");
        }
    }

    if (not opts.forwarded_args.empty()) {
        argv.emplace_back("--");
        argv.insert(argv.end(), opts.forwarded_args.begin(), opts.forwarded_args.end());
    }

    return argv;
}

/**
 * @brief   Run Baldr in a container.
 *
 * The resolved project directory is mounted to `/workspace`, baldr's own binary
 * is mounted in unless disabled via `cfg.docker_mount_baldr`, and the container
 * process runs as the host's uid:gid.
 */
[[nodiscard]] auto run_in_container(const options& opts, const baldr::config& cfg) -> int {
    std::vector<baldr::bind_mount> mounts;

    mounts.push_back({
        .host = std::filesystem::absolute(opts.project_dir).string(),
        .container = "/workspace",
        .read_only = false,
    });

    auto baldr_path = std::string{ "baldr" };
    if (cfg.docker_mount_baldr) {
        const auto host_path = cfg.docker_baldr_path.has_value()
            ? std::filesystem::path{ *cfg.docker_baldr_path }
            : self_exe_path();

        baldr_path = "/usr/local/bin/baldr";
        mounts.push_back({
            .host = host_path.string(),
            .container = baldr_path,
            .read_only = true
        });
    }

    const auto user = fmt::format("{}:{}", getuid(), getgid());
    const auto argv = build_container_argv(opts, baldr_path);

    return baldr::docker_run(
        *opts.image,
        argv,
        baldr::container_config {
            mounts,
            user
        }
    );
}

} // namespace

/**
 * @brief   Baldr CLI entry point, invoked via `NOVA_MAIN`.
 *
 * @param   args    Command line arguments (`argv[0]` included), as a range
 *                  of `std::string_view`.
 */
auto entrypoint(auto args) -> int {
    utl::rlog::init("baldr");

    std::vector<std::string> args_vec;
    for (const auto& arg: args | std::views::drop(1)) {
        args_vec.emplace_back(arg);
    }

    if (args_vec.empty()) {
        print_help(std::cerr, build_options_description());
        return EXIT_FAILURE;
    }

    const auto options = parse_args(args_vec);
    if (not options) {
        return EXIT_SUCCESS;
    }

    // See doc/baldr/user-guide.adoc for what this covers, and
    // doc/baldr/developer-manual.adoc for why signal_guard/signal_handler
    // are separate types.
    auto sigint = utl::signal_guard{ SIGINT, &utl::signal_handler::handle };

    int result = EXIT_SUCCESS;
    try {
        switch (options->command) {
            case command_type::build:
            case command_type::run: {
                auto cfg = baldr::load(options->project_dir);
                if (not cfg) {
                    throw nova::exception("Failed to load .baldr.yaml: {}", cfg.error().message);
                }

                auto merged_cfg = *cfg;
                if (options->build_type_explicit) {
                    merged_cfg.build_type = options->build_type;
                }
                for (const auto& [key, value]: options->cmake_defines) {
                    merged_cfg.cmake_defines[key] = value;
                }

                if (options->image) {
                    result = run_in_container(*options, merged_cfg);
                    break;
                }

                auto builder = baldr::builder{ options->project_dir, merged_cfg, options->build_dir };

                if (options->command == command_type::build) {
                    builder.build(options->target.value_or(""), options->clean_build);
                } else if (options->exec) {
                    builder.run_exec(*options->exec, options->forwarded_args, options->debug);
                } else {
                    builder.build(*options->target, options->clean_build);
                    builder.run(*options->target, options->forwarded_args, options->debug);
                }
                break;
            }
        }
    } catch (const nova::exception& ex) {
        if (not utl::signal_handler::triggered(SIGINT)) {
            utl::rlog::failure(ex.what());
        }
        result = EXIT_FAILURE;
    }

    if (utl::signal_handler::triggered(SIGINT)) {
        sigint.reraise_default();
    }

    return result;
}

NOVA_MAIN(entrypoint);
