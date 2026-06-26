#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_BIN="${DPC_BUILD_DIR:-${ROOT}/build}/bin"
"${ROOT}/scripts/build.sh" >/dev/null
TMP="$(mktemp -d)"
PORT="${DPC_DEMO_PORT:-19090}"
SERVER_PID=""
trap 'if [[ -n "${SERVER_PID}" ]]; then kill "${SERVER_PID}" 2>/dev/null || true; wait "${SERVER_PID}" 2>/dev/null || true; fi; rm -rf "${TMP}"' EXIT

wait_for_server() {
  local attempts=100
  for ((i = 0; i < attempts; i++)); do
    if (echo >"/dev/tcp/127.0.0.1/${PORT}") >/dev/null 2>&1; then
      return 0
    fi
    if ! kill -0 "${SERVER_PID}" 2>/dev/null; then
      echo "server exited before becoming ready" >&2
      cat "${TMP}/server.log" >&2 || true
      return 1
    fi
    sleep 0.05
  done
  echo "server did not become ready" >&2
  cat "${TMP}/server.log" >&2 || true
  return 1
}

mkdir -p "${TMP}/source"
printf 'network transfer demo\n' > "${TMP}/source/readme.txt"
dd if=/dev/zero of="${TMP}/source/large.bin" bs=1024 count=900 status=none

"${BUILD_BIN}/backup-server" --repo "${TMP}/repo" --port "${PORT}" --threads 4 >"${TMP}/server.log" 2>&1 &
SERVER_PID=$!
wait_for_server

"${BUILD_BIN}/backup-client" upload --source "${TMP}/source" --server "127.0.0.1:${PORT}" --session normal-session
"${BUILD_BIN}/backupctl" verify --repo "${TMP}/repo" --version 1

set +e
"${BUILD_BIN}/backup-client" upload --source "${TMP}/source" --server "127.0.0.1:${PORT}" --session interrupted-session --exit-after-chunks 10
rc=$?
set -e
test "${rc}" -ne 0

"${BUILD_BIN}/backup-client" upload --source "${TMP}/source" --server "127.0.0.1:${PORT}" --session interrupted-session
"${BUILD_BIN}/backupctl" list --repo "${TMP}/repo" | grep -q 'version: 2'
"${BUILD_BIN}/backupctl" verify --repo "${TMP}/repo" --version 2
"${BUILD_BIN}/backupctl" restore --repo "${TMP}/repo" --version 2 --target "${TMP}/restore"
diff -r "${TMP}/source" "${TMP}/restore"
kill "${SERVER_PID}" 2>/dev/null || true
wait "${SERVER_PID}" 2>/dev/null || true
SERVER_PID=""
echo "client/server demo ok"
