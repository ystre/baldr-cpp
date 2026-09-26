#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(dirname "$(realpath "$0")")"
PROJECT_DIR=$(git -C "${SCRIPT_DIR}" rev-parse --show-toplevel)

echo "=== Stage: Prepare ==="
echo "Clean copying repository content to ${WORKSPACE}..."
rm -rf "${WORKSPACE}"
cp --recursive "${PROJECT_DIR}" "${WORKSPACE}"
