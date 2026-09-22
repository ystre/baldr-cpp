#!/usr/bin/env bash

set -euo pipefail

OUT_DIR=package
INSTALL_DIR="${OUT_DIR}/install"

function build_id_stamp() {
    DEBUG_INFO_TMP="${WORKSPACE}/tmp/dbginf"
    mkdir --parent "${DEBUG_INFO_TMP}"

    readarray -d '' debug_files < <(find "${OUT_DIR}" -type f -name '*.debug' -print0)
    for f in ${debug_files[@]}; do
        build_id_stamp_one "$f"
    done
}

function build_id_stamp_one() {
    local f="$1"

    local build_id=$(readelf -n "$f" | grep 'Build ID' | awk '{print $3}')
    local prefix="${build_id:0:2}"
    local suffix="${build_id:2}"
    local debug_info_path="${DEBUG_INFO_TMP}/lib/debug/.build-id/$prefix"

    mkdir --parent "${debug_info_path}"
    mv "${f}" "${debug_info_path}/${suffix}.debug"
}

echo "=== Stage: Package ==="
mkdir --parent "${INSTALL_DIR}"
cmake --install build/release --prefix "${INSTALL_DIR}" --component baldr

"${WORKSPACE}/tools/debug-info.sh" "${OUT_DIR}"
build_id_stamp
tar -czf "${WORKSPACE}/debug.tgz" -C "${DEBUG_INFO_TMP}" lib/debug

cp "${WORKSPACE}/tools/install-script.sh" "${OUT_DIR}"
makeself "${OUT_DIR}" "${OUT_DIR}/baldr-installer" "Baldr" "./install-script.sh"

mv "${WORKSPACE}/debug.tgz" "${WORKSPACE}/package/debug.tgz"
