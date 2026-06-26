#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_BIN="${DPC_BUILD_DIR:-${ROOT}/build}/bin"
"${ROOT}/scripts/build.sh" >/dev/null
"${BUILD_BIN}/backup-bench" --workload large-file --size "${DPC_BENCH_SIZE:-64M}" --chunking "${DPC_BENCH_CHUNKING:-fixed}"
