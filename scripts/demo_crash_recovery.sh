#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
"${ROOT}/scripts/build.sh" >/dev/null
"${ROOT}/tests/fault_injection/crash_recovery_test.sh"
echo "crash recovery demo ok"
