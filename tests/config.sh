#!/usr/bin/env bash

# Config files: every format is found, the global and the project-local one
# merge whatever their formats, and a broken setup fails with a clear error.
#
# Runs a copy of the CMake test project with `$HOME` pointing to an empty
# directory; `hello` prints `BALDR_TEST_DEFINE`, set from the config files.

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
TEST_DIR="${PROJECT_DIR}/tests"
BALDR="${PROJECT_DIR}/build/debug/baldr/baldr"

WORK_DIR="${PROJECT_DIR}/build/test-config"
FAKE_HOME="${WORK_DIR}/home"
PROJECT="${WORK_DIR}/project"
BUILD_DIR="${WORK_DIR}/build"

function fail() {
    >&2 echo "FAILED: $*"
    exit 1
}

# An empty `$HOME` and a fresh copy of the test project, without any config;
# the build directory is kept to spare reconfiguring from scratch
function setup() {
    chmod -f u+rwx "${FAKE_HOME}" || true
    rm -rf "${FAKE_HOME}" "${PROJECT}"
    mkdir -p "${FAKE_HOME}" "${PROJECT}"
    cp -r "${TEST_DIR}/cmake-project/CMakeLists.txt" "${TEST_DIR}/cmake-project/src" "${PROJECT}"
}

function baldr() {
    HOME="${FAKE_HOME}" "${BALDR}" -p "${PROJECT}" "$@"
}

# Runs `hello`, expecting it to print `BALDR_TEST_DEFINE=$1`
function expect_define() {
    local expected="BALDR_TEST_DEFINE=$1"
    local output

    output=$(baldr run -t hello --build-dir "${BUILD_DIR}" 2>&1) || fail "baldr failed: ${output}"
    grep -qxF -- "${expected}" <<< "${output}" || fail "expected \`${expected}\` in: ${output}"
}

# Runs `hello`, expecting it not to print `BALDR_TEST_DEFINE`
function expect_no_define() {
    local output

    output=$(baldr run -t hello --build-dir "${BUILD_DIR}" 2>&1) || fail "baldr failed: ${output}"
    if grep -qF -- "BALDR_TEST_DEFINE=" <<< "${output}"; then
        fail "expected no \`BALDR_TEST_DEFINE\` in: ${output}"
    fi
}

# Expects Baldr to fail, with `$1` in its output
function expect_error() {
    local expected=$1
    local output

    if output=$(baldr build --build-dir "${BUILD_DIR}" 2>&1); then
        fail "baldr succeeded, but it should have failed with \`${expected}\`"
    fi
    grep -qF -- "${expected}" <<< "${output}" || fail "expected \`${expected}\` in: ${output}"
}

echo "Config: every format"

setup
printf 'cmake:\n  definitions:\n    BALDR_TEST_DEFINE: from-yaml\n' > "${PROJECT}/.baldr.yaml"
expect_define from-yaml

setup
printf 'cmake:\n  definitions:\n    BALDR_TEST_DEFINE: from-yml\n' > "${PROJECT}/.baldr.yml"
expect_define from-yml

setup
printf '{ "cmake": { "definitions": { "BALDR_TEST_DEFINE": "from-json" } } }\n' > "${PROJECT}/.baldr.json"
expect_define from-json

setup
printf "[cmake.definitions]\nBALDR_TEST_DEFINE = 'from-toml'\n" > "${PROJECT}/.baldr.toml"
expect_define from-toml

echo "Config: global and project-local in different formats"

setup
printf "[cmake.definitions]\nBALDR_TEST_DEFINE = 'global'\n" > "${FAKE_HOME}/.baldr.toml"
expect_define global

printf '{ "cmake": { "definitions": { "BALDR_TEST_DEFINE": "local" } } }\n' > "${PROJECT}/.baldr.json"
expect_define local

echo "Config: a deleted define leaves an existing build directory"

setup
printf 'cmake:\n  definitions:\n    BALDR_TEST_DEFINE: global\n' > "${FAKE_HOME}/.baldr.yaml"
expect_define global

printf 'cmake:\n  definitions:\n    BALDR_TEST_DEFINE:\n' > "${PROJECT}/.baldr.yaml"
expect_no_define

echo "Config: errors"

setup
printf 'debugger: lldb\n' > "${PROJECT}/.baldr.yaml"
printf "debugger = 'gdb'\n" > "${PROJECT}/.baldr.toml"
expect_error "Multiple config files in \`${PROJECT}\`: .baldr.yaml, .baldr.toml; keep only one"

setup
printf "debugger = 'gdb'\n" > "${FAKE_HOME}/.baldr.toml"
printf 'debugger: lldb\n' > "${FAKE_HOME}/.baldr.yml"
expect_error "Multiple config files in \`${FAKE_HOME}\`"

setup
printf '{ "debugger": \n' > "${PROJECT}/.baldr.json"
expect_error "Failed to read config"

setup
printf -- '- gdb\n- lldb\n' > "${PROJECT}/.baldr.yaml"
expect_error "Not a map"

setup
printf 'debugger-args: --args\n' > "${PROJECT}/.baldr.yaml"
expect_error "Config \`debugger-args\`: expected seq of string, got string"

# Permissions don't apply to root
if [[ "$(id -u)" -ne 0 ]]; then
    setup
    chmod 000 "${FAKE_HOME}"
    expect_error "Cannot access \`${FAKE_HOME}/.baldr.yaml\`"
fi

echo "Config: all passed"
