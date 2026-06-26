#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_BIN="${DPC_BUILD_DIR:-${ROOT}/build}/bin"
TMP="$(mktemp -d)"
trap 'rm -rf "${TMP}"' EXIT

hex() {
  printf '%s' "$1" | od -An -tx1 | tr -d ' \n'
}

make_repo() {
  local name="$1"
  local dir="${TMP}/${name}"
  mkdir -p "${dir}/source"
  printf 'alpha\n' > "${dir}/source/file.txt"
  dd if=/dev/zero of="${dir}/source/blob.bin" bs=1024 count=8 status=none
  "${BUILD_BIN}/backupctl" create --source "${dir}/source" --repo "${dir}/repo" >/dev/null
  printf '%s\n' "${dir}/repo"
}

expect_fail() {
  local out="${TMP}/command.out"
  local err="${TMP}/command.err"
  if "$@" >"${out}" 2>"${err}"; then
    echo "expected command to fail: $*" >&2
    exit 1
  fi
  if grep -qi 'segmentation fault\|core dumped' "${out}" "${err}"; then
    echo "command crashed instead of failing cleanly: $*" >&2
    cat "${out}" "${err}" >&2
    exit 1
  fi
}

first_object_rel() {
  local repo="$1"
  local object
  object="$(find "${repo}/objects" -type f -name '*.zst' | sort | head -n 1)"
  local sha
  sha="$(basename "${object}" .zst)"
  printf 'objects/%s/%s.zst\n' "${sha:0:2}" "${sha}"
}

repo="$(make_repo path-traversal)"
manifest="${repo}/manifests/version-000001.manifest"
sed -i "s/$(hex 'file.txt')/$(hex '../evil.txt')/" "${manifest}"
expect_fail "${BUILD_BIN}/backupctl" restore --repo "${repo}" --version 1 --target "${TMP}/restore-traversal"
test ! -e "${TMP}/evil.txt"

repo="$(make_repo object-traversal)"
manifest="${repo}/manifests/version-000001.manifest"
rel="$(first_object_rel "${repo}")"
sed -i "s/$(hex "${rel}")/$(hex '../../evil.zst')/" "${manifest}"
expect_fail "${BUILD_BIN}/backupctl" verify --repo "${repo}" --version 1

repo="$(make_repo object-mismatch)"
manifest="${repo}/manifests/version-000001.manifest"
rel="$(first_object_rel "${repo}")"
sha="$(basename "${rel}" .zst)"
wrong_prefix="ff"
if [[ "${sha:0:2}" == "ff" ]]; then
  wrong_prefix="00"
fi
wrong_rel="objects/${wrong_prefix}/${sha}.zst"
sed -i "s/$(hex "${rel}")/$(hex "${wrong_rel}")/" "${manifest}"
expect_fail "${BUILD_BIN}/backupctl" verify --repo "${repo}" --version 1

repo="$(make_repo missing-object)"
object="$(find "${repo}/objects" -type f -name '*.zst' | sort | head -n 1)"
rm -f "${object}"
expect_fail "${BUILD_BIN}/backupctl" verify --repo "${repo}" --version 1
expect_fail "${BUILD_BIN}/backupctl" restore --repo "${repo}" --version 1 --target "${TMP}/restore-missing"

repo="$(make_repo corrupt-object)"
object="$(find "${repo}/objects" -type f -name '*.zst' | sort | head -n 1)"
printf 'not a zstd frame' > "${object}"
expect_fail "${BUILD_BIN}/backupctl" verify --repo "${repo}" --version 1
expect_fail "${BUILD_BIN}/backupctl" restore --repo "${repo}" --version 1 --target "${TMP}/restore-corrupt"

repo="$(make_repo truncated-manifest)"
printf 'DPC_MANIFEST_V1\nversion 1\n' > "${repo}/manifests/version-000001.manifest"
expect_fail "${BUILD_BIN}/backupctl" verify --repo "${repo}" --version 1

repo="$(make_repo wal-crc)"
printf '\xff' | dd of="${repo}/metadata/wal.log" bs=1 seek=20 count=1 conv=notrunc status=none
expect_fail "${BUILD_BIN}/backupctl" recover --repo "${repo}"

repo="${TMP}/wal-truncated/repo"
mkdir -p "${repo}/metadata"
printf 'bad' > "${repo}/metadata/wal.log"
expect_fail "${BUILD_BIN}/backupctl" recover --repo "${repo}"

echo "security malformed input tests ok"
