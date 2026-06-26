#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

docker build --target test -t cpp-data-protection-core:test .
docker run --rm cpp-data-protection-core:test bash -lc './scripts/test.sh && ./scripts/demo_local_backup.sh && ./scripts/demo_crash_recovery.sh'
