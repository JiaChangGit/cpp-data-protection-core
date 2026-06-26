#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

docker compose -p cpp-data-protection-core down -v --remove-orphans || true

if [[ "${1:-}" == "--images" ]]; then
  docker image rm cpp-data-protection-core:dev cpp-data-protection-core:test cpp-data-protection-core:runtime 2>/dev/null || true
fi
