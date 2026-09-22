#!/usr/bin/env bash

set -euo pipefail

BALDR=baldr

echo "=== Stage: Build ==="
git submodule update --init
"${BALDR}" build -DSANITIZERS=asan
