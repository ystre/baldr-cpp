#!/usr/bin/env bash

set -euo pipefail

BUILD_DIR="${WORKSPACE}/build/debug"
COVERAGE_DIR="${BUILD_DIR}/coverage"

echo "=== Stage: Coverage ==="
mkdir --parent "${COVERAGE_DIR}"

gcovr "${BUILD_DIR}" --root "${WORKSPACE}" --exclude '.*test.*' --exclude '.*_deps.*' --cobertura --output "${COVERAGE_DIR}/cobertura.xml"
gcovr "${BUILD_DIR}" --root "${WORKSPACE}" --exclude '.*test.*' --exclude '.*_deps.*' --html-details --output "${COVERAGE_DIR}/baldr.html"
