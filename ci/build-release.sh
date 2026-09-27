#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${WORKSPACE}/build/release"
echo "=== Stage: Release Build ==="

cmake -S "${WORKSPACE}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES="${WORKSPACE}/env/conan_provider.cmake" \
    -DBALDR_STATIC_LINK=ON

cmake --build "${BUILD_DIR}"
