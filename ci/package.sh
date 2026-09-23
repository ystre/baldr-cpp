#!/usr/bin/env bash

set -euo pipefail

OUT_DIR=package
INSTALL_DIR="${OUT_DIR}/install"

echo "=== Stage: Package ==="
mkdir --parent "${INSTALL_DIR}"
cmake --install ${WORKSPACE}/build/release --prefix "${INSTALL_DIR}" --component baldr

cp "${WORKSPACE}/tools/install-script.sh" "${OUT_DIR}"
makeself "${OUT_DIR}" "${OUT_DIR}/baldr-installer" "Baldr" "./install-script.sh"
