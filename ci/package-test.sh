#!/usr/bin/env bash

set -euo pipefail

echo "=== Stage: Package Test ==="
INSTALL_PATH="${WORKSPACE}/package-test" "${WORKSPACE}/package/baldr-installer"
tree "${WORKSPACE}/package-test"
test -f "${WORKSPACE}/package-test/share/baldr/classifier.toml"
