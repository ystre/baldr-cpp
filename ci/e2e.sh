#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

export WORKSPACE=/tmp/workspace/baldr
mkdir --parent "${WORKSPACE}"

"${PROJECT_DIR}/ci/prepare.sh"
cd "${WORKSPACE}"

"${PROJECT_DIR}/ci/init.sh"
"${PROJECT_DIR}/ci/build.sh"
"${PROJECT_DIR}/ci/unit-test.sh"
"${PROJECT_DIR}/ci/function-test.sh"
"${PROJECT_DIR}/ci/build-release.sh"
"${PROJECT_DIR}/ci/package.sh"
"${PROJECT_DIR}/ci/package-test.sh"
"${PROJECT_DIR}/ci/summary.sh"
