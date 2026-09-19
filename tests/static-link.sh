#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
BALDR="${PROJECT_DIR}/build/release/baldr/baldr"
STATIC_BIN="${PROJECT_DIR}/build/static/baldr/baldr"

echo "Building baldr statically into build/static..."
"$BALDR" -p "${PROJECT_DIR}" build -t baldr \
    --build-type Release \
    --build-dir build/static \
    -D BALDR_STATIC_LINK=ON

echo "Verifying ${STATIC_BIN} has no dynamic dependencies..."
if ldd "${STATIC_BIN}" >/dev/null 2>&1; then
    echo "ERROR: ${STATIC_BIN} is dynamically linked" >&2
    exit 1
fi

echo "Running it inside a bare Alpine container..."
CONTEXT_DIR=$(mktemp -d)
trap 'rm -rf "${CONTEXT_DIR}"' EXIT

cp "${SCRIPT_DIR}/alpine.Dockerfile" "${CONTEXT_DIR}/"
cp "${STATIC_BIN}" "${CONTEXT_DIR}/baldr"

docker build -f "${CONTEXT_DIR}/alpine.Dockerfile" -t baldr-static-test "${CONTEXT_DIR}" --progress=plain
docker run --rm baldr-static-test --version

echo "OK: static build runs with zero runtime libraries present."
