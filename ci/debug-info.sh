#!/usr/bin/env bash

set -euo pipefail

DEBUG_INFO_TMP="${WORKSPACE}/tmp/dbginf"

echo "=== Stage: Creating Debug Info ==="
mkdir "${DEBUG_INFO_TMP}"
cp -r "${WORKSPACE}/package/install" "${DEBUG_INFO_TMP}"
"${WORKSPACE}/tools/debug-info.sh" "${DEBUG_INFO_TMP}"
tar -czf "${WORKSPACE}/package/debug.tgz" -C "${DEBUG_INFO_TMP}" lib/debug
