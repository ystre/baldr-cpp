#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${WORKSPACE}/build/debug"

echo "=== Stage: Unit Test ==="
ctest --test-dir "${BUILD_DIR}" --build-config Debug --output-on-failure
