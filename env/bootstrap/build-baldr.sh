#!/usr/bin/env bash

set -euo pipefail

cmake -S "${WORKSPACE}" \
    -B "${WORKSPACE}/build/docker" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=/tmp/workspace/conan_provider.cmake \
    -DBALDR_STATIC_LINK=ON

cmake --build "${WORKSPACE}/build/docker"
