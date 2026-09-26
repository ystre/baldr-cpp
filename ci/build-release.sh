#!/usr/bin/env bash

set -euo pipefail

BALDR=baldr
echo "=== Stage: Release Build ==="
"${BALDR}" build \
    --build-type Release \
    -DBALDR_STATIC_LINK=ON
