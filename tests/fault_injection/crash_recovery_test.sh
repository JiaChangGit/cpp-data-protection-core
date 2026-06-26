#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${DPC_BUILD_DIR:-${ROOT}/build}/bin"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

mkdir -p "${TMP}/source"
printf 'crash recovery data\n' > "${TMP}/source/file.txt"
dd if=/dev/zero of="${TMP}/source/blob.bin" bs=1024 count=80 status=none

expect_no_version() {
  local repo="$1"
  "${BUILD_DIR}/backupctl" recover --repo "${repo}"
  "${BUILD_DIR}/backupctl" list --repo "${repo}" | grep -q 'no committed versions'
}

for stage in after-begin after-object-write after-manifest-write after-manifest-rename; do
  repo="${TMP}/repo-${stage}"
  set +e
  "${BUILD_DIR}/backupctl" create --source "${TMP}/source" --repo "${repo}" --fault-stage "${stage}" >/tmp/dpc_fault.out 2>/tmp/dpc_fault.err
  rc=$?
  set -e
  test "${rc}" -ne 0
  expect_no_version "${repo}"
done

repo="${TMP}/repo-after-commit-marker"
set +e
"${BUILD_DIR}/backupctl" create --source "${TMP}/source" --repo "${repo}" --fault-stage after-commit-marker >/tmp/dpc_fault.out 2>/tmp/dpc_fault.err
rc=$?
set -e
test "${rc}" -ne 0
"${BUILD_DIR}/backupctl" recover --repo "${repo}"
"${BUILD_DIR}/backupctl" list --repo "${repo}" | grep -q 'version: 1'
"${BUILD_DIR}/backupctl" verify --repo "${repo}" --version 1
