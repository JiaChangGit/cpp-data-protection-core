#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${DPC_BUILD_DIR:-${ROOT}/build}/bin"
"${ROOT}/scripts/build.sh" >/dev/null
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

mkdir -p "${TMP}/source/docs"
printf 'hello data protection\n' > "${TMP}/source/readme.txt"
printf 'nested file\n' > "${TMP}/source/docs/nested.txt"
dd if=/dev/zero of="${TMP}/source/repeated.bin" bs=1024 count=192 status=none

"${BUILD_DIR}/backupctl" create --source "${TMP}/source" --repo "${TMP}/repo" --chunking fixed
"${BUILD_DIR}/backupctl" list --repo "${TMP}/repo"
"${BUILD_DIR}/backupctl" verify --repo "${TMP}/repo" --version 1
"${BUILD_DIR}/backupctl" restore --repo "${TMP}/repo" --version 1 --target "${TMP}/restore"
diff -r "${TMP}/source" "${TMP}/restore"
echo "local backup demo ok"
