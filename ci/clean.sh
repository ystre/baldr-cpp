#!/usr/bin/env bash

set -euo pipefail

echo "=== Stage: Clean ==="
git clean -xdff
git reset --hard HEAD
