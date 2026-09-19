#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

"${SCRIPT_DIR}/smoke.sh"
"${SCRIPT_DIR}/static-link.sh"
"${SCRIPT_DIR}/glibc.sh"
