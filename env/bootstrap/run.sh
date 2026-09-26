#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

WORKSPACE=/tmp/workspace/baldr
CONAN_CACHE="${HOME}/.conan2/p"

docker run --rm \
    --user $(id -u):$(id -g) \
    --volume "${CONAN_CACHE}":"/tmp/workspace/.conan2/p" \
    --volume "${PROJECT_DIR}":"${WORKSPACE}" \
    --env WORKSPACE="${WORKSPACE}" \
    --env CONAN_HOME=/tmp/workspace/.conan2 \
    --workdir "${WORKSPACE}" \
    cpp-toolchain:latest \
    sh -c 'cd ${WORKSPACE}/env/bootstrap && ./install.nova.sh && ./build-baldr.sh'
