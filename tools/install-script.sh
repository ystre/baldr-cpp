#!/usr/bin/env sh

set -euo pipefail

if [ -z "${INSTALL_PATH:-""}" ]; then
    INSTALL_PATH="${HOME}/.local"
fi

if [ ! -e "$INSTALL_PATH" ]; then
    mkdir --parent "$INSTALL_PATH"
fi

cp --recursive ./install/* "$INSTALL_PATH"

echo "Baldr has been installed to: $INSTALL_PATH"
echo "Verifying..."
"$INSTALL_PATH/bin/baldr" --version
