#!/usr/bin/env bash

set -euo pipefail

BALDR=baldr

echo "=== Stage: Build ==="
"${BALDR}" build
