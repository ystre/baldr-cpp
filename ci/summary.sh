#!/usr/bin/env bash

set -euo pipefail

echo "=== Stage: Summary ==="
tree "$PWD/package"

tar --list -f "${WORKSPACE}/package/debug.tgz"
