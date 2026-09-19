#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
PROJECT_DIR="${SCRIPT_DIR}/.."
TMP_DIR="${PROJECT_DIR}/.tmp"
INSTALL_DIR="${TMP_DIR}/install"

mkdir --parent "${INSTALL_DIR}"

baldr build -b Release
cmake --install build/release --prefix "${INSTALL_DIR}" --component baldr

makeself "${INSTALL_DIR}" "${TMP_DIR}/baldr-installer" "Baldr" "${PROJECT_DIR}/tools/install-script.sh"
