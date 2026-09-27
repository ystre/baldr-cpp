#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${WORKSPACE}/build/debug"

echo "=== Stage: Build ==="
git submodule update --init
conan export "${WORKSPACE}/deps/nova-cpp/libnova"

cmake -S "${WORKSPACE}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DSANITIZERS=asan \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="${WORKSPACE}/env/conan_provider.cmake"

cmake --build "${BUILD_DIR}"
