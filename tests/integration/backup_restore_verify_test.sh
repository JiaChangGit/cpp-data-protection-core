#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${DPC_BUILD_DIR:-${ROOT}/build}/bin"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

"${BUILD_DIR}/backupctl" create --source "${TMP}/source-missing" --repo "${TMP}/repo" >/dev/null 2>&1 && {
  echo "expected missing source to fail" >&2
  exit 1
} || true

mkdir -p "${TMP}/source/dir"
printf 'alpha\n' > "${TMP}/source/a.txt"
printf 'beta\n' > "${TMP}/source/dir/b.txt"
dd if=/dev/zero of="${TMP}/source/blob.bin" bs=1024 count=160 status=none

"${BUILD_DIR}/backupctl" create --source "${TMP}/source" --repo "${TMP}/repo" --chunking fixed
"${BUILD_DIR}/backupctl" list --repo "${TMP}/repo" | grep -q 'version: 1'
"${BUILD_DIR}/backupctl" verify --repo "${TMP}/repo" --version 1
"${BUILD_DIR}/backupctl" restore --repo "${TMP}/repo" --version 1 --target "${TMP}/restore"
diff -r "${TMP}/source" "${TMP}/restore"

"${BUILD_DIR}/backupctl" create --source "${TMP}/source" --repo "${TMP}/repo-cdc" --chunking cdc
"${BUILD_DIR}/backupctl" verify --repo "${TMP}/repo-cdc" --version 1
"${BUILD_DIR}/backupctl" restore --repo "${TMP}/repo-cdc" --version 1 --target "${TMP}/restore-cdc"
diff -r "${TMP}/source" "${TMP}/restore-cdc"

printf 'alpha changed\n' > "${TMP}/source/a.txt"
"${BUILD_DIR}/backupctl" create --source "${TMP}/source" --repo "${TMP}/repo" --chunking fixed
"${BUILD_DIR}/backupctl" verify --repo "${TMP}/repo" --version 2
"${BUILD_DIR}/backupctl" checkpoint --repo "${TMP}/repo"
"${BUILD_DIR}/backupctl" compact --repo "${TMP}/repo"
"${BUILD_DIR}/backupctl" recover --repo "${TMP}/repo"
"${BUILD_DIR}/backupctl" verify --repo "${TMP}/repo" --version 2
STATS="$("${BUILD_DIR}/backupctl" stats --repo "${TMP}/repo")"
echo "${STATS}"
echo "${STATS}" | grep -Eq 'duplicate_chunks: [1-9][0-9]*'
