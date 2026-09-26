#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)
TEST_DIR="${PROJECT_DIR}/tests"
BUILD_DIR="${PROJECT_DIR}/build"
BALDR="${PROJECT_DIR}/build/debug/baldr/baldr"

# In case of make project, makefile controls the build directory
$BALDR -p "${TEST_DIR}/make-project" run -t hello

$BALDR -p "${TEST_DIR}/cmake-project" run -t hello --build-dir "${BUILD_DIR}/test-cmake"
$BALDR -p "${TEST_DIR}/cmake-project" run -t hello -b Release --build-dir "${BUILD_DIR}/test-cmake"
$BALDR -p "${TEST_DIR}/cmake-project" run -t hello -b Release --clean --build-dir "${BUILD_DIR}/test-cmake"
$BALDR -p "${TEST_DIR}/cmake-project" run -t hello --build-dir "${BUILD_DIR}/test-cmake" -- --foo bar
$BALDR -p "${TEST_DIR}/cmake-project" run -t hello -b Release -D BALDR_TEST_DEFINE=1 --build-dir "${BUILD_DIR}/test-cmake"
$BALDR -p "${TEST_DIR}/cmake-project" run -t hello -b Release -D BALDR_TEST_DEFINE=2 --build-dir "${BUILD_DIR}/test-cmake"
$BALDR -p "${TEST_DIR}/cmake-project" run -t hello -b Release -DBALDR_TEST_DEFINE=3 --build-dir "${BUILD_DIR}/test-cmake"
