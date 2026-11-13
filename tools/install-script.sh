#!/usr/bin/env bash
#
# Installs Baldr from the unpacked package.
#
# Usage:
#   baldr-installer [-- --completion|--no-completion]
#
# INSTALL_PATH sets the prefix (default: ~/.local). Shell completion for bash
# and zsh is installed on request: asked on a terminal, skipped otherwise.

set -euo pipefail

INSTALL_PATH="${INSTALL_PATH:-${HOME}/.local}"

usage() {
    echo "Usage: $0 [--completion|--no-completion]"
}

# Prints `yes` or `no`: the flag if given, the answer on a terminal, else `no`
wants_completion() {
    case "${1:-}" in
        --completion)       echo yes; return ;;
        --no-completion)    echo no; return ;;
    esac

    if [[ ! -t 0 ]]; then
        echo no
        return
    fi

    local answer
    read -r -p "Install shell completion for bash and zsh? [Y/n] " answer
    case "${answer}" in
        ""|[Yy]*)   echo yes ;;
        *)          echo no ;;
    esac
}

# Whether `dir` is on the zsh function path, as far as the zshrc tells
zsh_fpath_has() {
    local dir="$1"
    local zshrc="${ZDOTDIR:-${HOME}}/.zshrc"

    [[ "${dir}" == /usr/share/* || "${dir}" == /usr/local/share/* ]] && return 0
    [[ -f "${zshrc}" ]] && grep --quiet --fixed-strings "${dir#"${HOME}"}" "${zshrc}"
}

install_completion() {
    local source_dir="${INSTALL_PATH}/share/baldr/completion"
    local bash_dir="${INSTALL_PATH}/share/bash-completion/completions"
    local zsh_dir="${INSTALL_PATH}/share/zsh/site-functions"

    mkdir --parents "${bash_dir}" "${zsh_dir}"
    cp "${source_dir}/baldr.bash" "${bash_dir}/baldr"
    cp "${source_dir}/_baldr" "${zsh_dir}/_baldr"

    echo "Shell completion has been installed:"
    echo "  bash: ${bash_dir}/baldr (loaded by bash-completion)"
    echo "  zsh:  ${zsh_dir}/_baldr"

    if ! zsh_fpath_has "${zsh_dir}"; then
        echo "For zsh, add this line to your .zshrc before \`compinit\` runs:"
        echo "  fpath=(${zsh_dir} \$fpath)"
    fi
}

case "${1:-}" in
    ""|--completion|--no-completion) ;;
    -h|--help)  usage; exit 0 ;;
    *)          usage >&2; exit 1 ;;
esac

completion=$(wants_completion "${1:-}")

mkdir --parents "${INSTALL_PATH}"
cp --recursive ./install/* "${INSTALL_PATH}"

echo "Baldr has been installed to: ${INSTALL_PATH}"

if [[ "${completion}" == yes ]]; then
    install_completion
fi

echo "Verifying..."
"${INSTALL_PATH}/bin/baldr" --version
