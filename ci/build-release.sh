#!/usr/bin/env bash

set -euo pipefail

BALDR=baldr
BUILD_IMAGE=alpine:latest

echo "=== Stage: Release Build ==="
docker pull "${BUILD_IMAGE}"
"${BALDR}" build \
    --build-type Release \
    -DBALDR_STATIC_LINK=ON
