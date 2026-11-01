#!/usr/bin/env bash

set -euo pipefail

if [ -z "${INSTALL_PATH:-""}" ]; then
    INSTALL_PATH="${HOME}/.local"
fi

if [ ! -e "$INSTALL_PATH" ]; then
    mkdir -p "$INSTALL_PATH"
fi

cp -R ./install/* "$INSTALL_PATH"

echo "Baldr has been installed to: $INSTALL_PATH"
echo "Verifying..."
"$INSTALL_PATH/bin/baldr" --version
