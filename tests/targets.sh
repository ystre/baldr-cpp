#!/usr/bin/env bash

# `baldr targets` and the shell completion built on it: the listing of the
# CMake test project, nothing for a Makefile project, and the target names the
# bash and zsh completion offer for `build -t` and `run -t`.

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
TEST_DIR="${PROJECT_DIR}/tests"
BALDR="${PROJECT_DIR}/build/debug/baldr/baldr"

BUILD_DIR="${PROJECT_DIR}/build/test-targets"
BIN_DIR="${BUILD_DIR}/bin"

function fail() {
    >&2 echo "FAILED: $*"
    exit 1
}

function expect_eq() {
    local what="$1" expected="$2" actual="$3"
    [[ "${actual}" == "${expected}" ]] || fail "${what}: expected '${expected}', got '${actual}'"
}

rm -rf "${BUILD_DIR}"
mkdir -p "${BIN_DIR}"
ln -s "${BALDR}" "${BIN_DIR}/baldr"
export PATH="${BIN_DIR}:${PATH}"

project_opts=(-p "${TEST_DIR}/cmake-project" --build-dir "${BUILD_DIR}/cmake")

# The first listing configures, the second reads the existing reply
expect_eq "targets" $'hello\texecutable' "$(baldr "${project_opts[@]}" targets)"
expect_eq "targets again" $'hello\texecutable' "$(baldr "${project_opts[@]}" targets)"

expect_eq "make project" "" "$(baldr -p "${TEST_DIR}/make-project" targets 2>/dev/null)"

# A build directory configured without the code model request is reconfigured
rm -rf "${BUILD_DIR}/cmake/.cmake"
expect_eq "targets after lost query" $'hello\texecutable' "$(baldr "${project_opts[@]}" targets)"

function bash_complete() {
    bash -c '
        source "$1"
        shift
        COMP_WORDS=("$@")
        COMP_CWORD=$(($# - 1))
        _baldr
        echo "${COMPREPLY[*]}"
    ' bash "${PROJECT_DIR}/res/completion/baldr.bash" "$@"
}

expect_eq "bash commands" "build run targets" "$(bash_complete baldr "")"
expect_eq "bash run -t" "hello" "$(bash_complete baldr "${project_opts[@]}" run -t "")"
expect_eq "bash build -t" "hello" "$(bash_complete baldr "${project_opts[@]}" build -t h)"
expect_eq "bash -t without match" "" "$(bash_complete baldr "${project_opts[@]}" run -t x)"

zsh -n "${PROJECT_DIR}/res/completion/_baldr" || fail "zsh completion has a syntax error"

echo "PASSED: targets"
