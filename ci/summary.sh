#!/usr/bin/env bash

set -euo pipefail

echo "=== Stage: Summary ==="
tree "${WORKSPACE}/package"

echo "= Debug Package"
tar --list -f "${WORKSPACE}/package/debug.tgz"
