#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${DPC_BUILD_DIR:-${ROOT}/build}"
"${ROOT}/scripts/build.sh"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
"${ROOT}/tests/integration/backup_restore_verify_test.sh"
"${ROOT}/tests/fault_injection/crash_recovery_test.sh"
"${ROOT}/tests/security/security_malformed_test.sh"
