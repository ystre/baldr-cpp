#!/usr/bin/env bash

set -euo pipefail

BALDR=baldr

echo "=== Stage: Build ==="
git submodule update --init
conan export "${WORKSPACE}/deps/nova-cpp/libnova"
"${BALDR}" build
