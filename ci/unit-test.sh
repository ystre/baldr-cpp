#!/usr/bin/env bash

set -euo pipefail

BALDR=baldr

echo "=== Stage: Unit Test ==="
"${BALDR}" build --target test
