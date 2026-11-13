# Bash completion of Baldr: commands, options and the targets of -t/--target.

# The targets of the project, read with `baldr targets`. `run` gets the
# executables only. The project options pick the project and build directory.
_baldr_targets() {
    local command="$1"
    shift

    local entry
    while IFS= read -r entry; do
        if [[ "${command}" != run || "${entry#*$'\t'}" == executable ]]; then
            printf '%s\n' "${entry%%$'\t'*}"
        fi
    done < <(baldr "$@" targets 2>/dev/null)
}

_baldr() {
    local cur="${COMP_WORDS[COMP_CWORD]}"
    local prev="${COMP_WORDS[COMP_CWORD - 1]}"

    local command=""
    local -a project_opts=()
    local index word
    for ((index = 1; index < COMP_CWORD; index++)); do
        word="${COMP_WORDS[index]}"
        case "${word}" in
            -p|--project|-b|--build-type|--build-dir)
                project_opts+=("${word}" "${COMP_WORDS[index + 1]/#\~/${HOME}}")
                ((index++))
                ;;
            -D|--cmake-define|-t|--target|-i|--image)
                ((index++))
                ;;
            build|run|targets)
                command="${command:-${word}}"
                ;;
        esac
    done

    case "${prev}" in
        -t|--target)
            mapfile -t COMPREPLY < <(compgen -W "$(_baldr_targets "${command}" "${project_opts[@]}")" -- "${cur}")
            return
            ;;
        -p|--project|--build-dir)
            compopt -o filenames 2>/dev/null
            mapfile -t COMPREPLY < <(compgen -d -- "${cur}")
            return
            ;;
        -b|--build-type)
            mapfile -t COMPREPLY < <(compgen -W "Debug Release RelWithDebInfo MinSizeRel" -- "${cur}")
            return
            ;;
        -D|--cmake-define|-i|--image)
            COMPREPLY=()
            return
            ;;
    esac

    if [[ "${cur}" == -* ]]; then
        local options="-h --help -v --version -p --project -b --build-type --build-dir --clean
            -D --cmake-define -t --target --build --debug -i --image"
        mapfile -t COMPREPLY < <(compgen -W "${options}" -- "${cur}")
    elif [[ -z "${command}" ]]; then
        mapfile -t COMPREPLY < <(compgen -W "build run targets" -- "${cur}")
    else
        COMPREPLY=()
    fi
}

complete -F _baldr baldr
