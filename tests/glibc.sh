#!/usr/bin/env bash

set -eu

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
BALDR="${PROJECT_DIR}/build/debug/baldr/baldr"
STATIC_BALDR="${PROJECT_DIR}/build/static/baldr/baldr"
TEST_PROJECT="${SCRIPT_DIR}/make-project"
CONFIG_FILE="${TEST_PROJECT}/.baldr.yaml"
IMAGE="baldr-glibc-test"

export SPDLOG_MODE=standard

function glibc_version_of() {
    if [[ -z "$1" ]]; then
        ldd --version | head -n1 | grep -oE '[0-9]+\.[0-9]+$'
    else
        docker run --rm "$1" ldd --version | head -n1 | grep -oE '[0-9]+\.[0-9]+$'
    fi
}

function version_le() {
    [[ "$1" == "$2" ]] || [[ "$(printf '%s\n%s\n' "$1" "$2" | sort -V | head -n1)" == "$1" ]]
}

if [[ ! -x "${STATIC_BALDR}" ]]; then
    echo "Building a statically-linked baldr..."
    "$BALDR" -p "${PROJECT_DIR}" build -t baldr \
        --build-type Release \
        --build-dir build/static \
        -D BALDR_STATIC_LINK=ON
fi

function cleanup() {
    rm -f "${CONFIG_FILE}";
}

trap cleanup EXIT

cat > "${CONFIG_FILE}" <<EOF
docker:
  baldr-path: ${STATIC_BALDR}
EOF

echo "Building the toolchain image (${IMAGE})..."
docker build --quiet --tag "${IMAGE}" --file "${SCRIPT_DIR}/glibc.Dockerfile" "${SCRIPT_DIR}"

IMAGE_GLIBC=$(glibc_version_of "${IMAGE}")
HOST_GLIBC=$(glibc_version_of "")
BUILD_DIR="build"

echo "Image and host glibc version: ${IMAGE_GLIBC} / ${HOST_GLIBC}"

echo "Building test project via image..."
"$BALDR" -p "${TEST_PROJECT}" build -t hello \
    --image "${IMAGE}" \
    --build-dir "${BUILD_DIR}"

BIN="${TEST_PROJECT}/${BUILD_DIR}/hello"
if [[ ! -x "${BIN}" ]]; then
    echo "ERROR: ${BIN} was not built" >&2
    exit 1
fi

MAX_SYMBOL_VERSION=$(objdump -T "${BIN}" | grep -oE 'GLIBC_[0-9]+\.[0-9]+' | sed 's/GLIBC_//' | sort -V | tail -n1)

if [[ -z "${MAX_SYMBOL_VERSION}" ]]; then
    echo "ERROR: no GLIBC_x.y symbol versions found in built target!" >&2
    exit 1
fi

echo "glibc required by the built target: ${MAX_SYMBOL_VERSION}"

if version_le "${HOST_GLIBC}" "${MAX_SYMBOL_VERSION}"; then
    echo "ERROR: Built target requires the same GLIBC_${MAX_SYMBOL_VERSION} as the host!" >&2
    exit 1
fi

echo "Running built target"
"${BIN}" --smoke-test-arg

echo "Test passed"
