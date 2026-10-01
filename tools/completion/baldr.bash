# Bash completion for `baldr`.
#
# Source this file (e.g. from ~/.bashrc):
#   source /path/to/baldr.bash
#
# Completes:
#   - the <command> positional ('build', 'run')
#   - '-t/--target <name>' with the project's actual CMake target names,
#     via `baldr --list-targets` (passing through -p/-b/--build-dir so the
#     right project/build-dir is queried)

_baldr_forwarded_project_opts() {
    local words=("${COMP_WORDS[@]}")
    local opts=()
    local i

    for ((i = 1; i < ${#words[@]}; i++)); do
        case "${words[i]}" in
            -p|--project|-b|--build-type|--build-dir)
                if [[ -n "${words[i+1]:-}" ]]; then
                    opts+=("${words[i]}" "${words[i+1]}")
                fi
                ;;
        esac
    done

    printf '%s\n' "${opts[@]}"
}

_baldr_targets() {
    local -a opts
    mapfile -t opts < <(_baldr_forwarded_project_opts)
    baldr --list-targets "${opts[@]}" 2>/dev/null
}

_baldr() {
    local cur prev
    cur="${COMP_WORDS[COMP_CWORD]}"
    prev="${COMP_WORDS[COMP_CWORD-1]}"

    local commands="build run"
    local flags="-h --help -v --version -p --project -b --build-type --build-dir --clean -D --cmake-define -t --target -x --exec --build --debug -i --image --list-targets"

    case "$prev" in
        -t|--target)
            mapfile -t COMPREPLY < <(compgen -W "$(_baldr_targets)" -- "$cur")
            return
            ;;
        -p|--project|--build-dir|-x|--exec)
            COMPREPLY=($(compgen -f -- "$cur"))
            return
            ;;
        -b|--build-type)
            COMPREPLY=($(compgen -W "Debug Release RelWithDebInfo MinSizeRel" -- "$cur"))
            return
            ;;
        -i|--image|-D|--cmake-define)
            COMPREPLY=()
            return
            ;;
    esac

    if [[ "$cur" == -* ]]; then
        COMPREPLY=($(compgen -W "$flags" -- "$cur"))
        return
    fi

    # First non-flag token still unseen -> complete the command.
    local word
    for word in "${COMP_WORDS[@]:1:COMP_CWORD-1}"; do
        if [[ "$word" != -* ]]; then
            COMPREPLY=()
            return
        fi
    done

    COMPREPLY=($(compgen -W "$commands" -- "$cur"))
}

complete -F _baldr baldr
