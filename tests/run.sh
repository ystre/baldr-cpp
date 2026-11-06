#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

"${PROJECT_DIR}/build/debug/tests/classifier-check" "${PROJECT_DIR}/res/classifier.toml" "${SCRIPT_DIR}/classifier-lines.txt"
"${SCRIPT_DIR}/smoke.sh"
"${SCRIPT_DIR}/config.sh"
"${SCRIPT_DIR}/static-link.sh"
"${SCRIPT_DIR}/glibc.sh"
