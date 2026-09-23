#!/usr/bin/env bash

set -euo pipefail

echo "=== Stage: Function Test ==="
"${WORKSPACE}/tests/run.sh"
