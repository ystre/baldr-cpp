#compdef baldr
#
# Zsh completion for `baldr`.
#
# Either source it directly (e.g. from ~/.zshrc):
#   source /path/to/baldr.zsh
# or drop it on your $fpath as `_baldr` and let `compinit` pick it up.

_baldr_targets() {
    local -a opts
    local i words_count=${#words}

    for ((i = 1; i <= words_count; i++)); do
        case "${words[i]}" in
            -p|--project|-b|--build-type|--build-dir)
                if [[ -n "${words[i+1]:-}" ]]; then
                    opts+=("${words[i]}" "${words[i+1]}")
                fi
                ;;
        esac
    done

    local -a targets
    targets=("${(f)$(baldr --list-targets "${opts[@]}" 2>/dev/null)}")
    _describe 'target' targets
}

_baldr() {
    local -a commands
    commands=('build:Configure (if needed) and build the project' 'run:Run a built target or an arbitrary executable')

    _arguments -C \
        '(-h --help)'{-h,--help}'[Produce help message]' \
        '(-v --version)'{-v,--version}'[Print version and exit]' \
        '(-p --project)'{-p,--project}'[Project directory]:directory:_files -/' \
        '(-b --build-type)'{-b,--build-type}'[CMake build type]:build type:(Debug Release RelWithDebInfo MinSizeRel)' \
        '--build-dir[Override the default build directory]:directory:_files -/' \
        '--clean[Wipe the build directory before building]' \
        '(-D --cmake-define)'{-D,--cmake-define}'[CMake define KEY=VALUE]:define:' \
        '(-t --target)'{-t,--target}'[Executable name to run]:target:_baldr_targets' \
        '(-x --exec)'{-x,--exec}'[Arbitrary executable to run]:executable:_files' \
        '--build[Build the project first (for run)]' \
        '--debug[Launch the target under the configured debugger]' \
        '(-i --image)'{-i,--image}'[Docker image to use]:image:' \
        '--list-targets[List available CMake build targets and exit]' \
        '1: :->command' \
        '*::: :->args'

    case "$state" in
        command)
            _describe 'command' commands
            ;;
    esac
}

_baldr "$@"
