#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

cleanup() {
  docker compose -p cpp-data-protection-core down -v --remove-orphans >/dev/null 2>&1 || true
}
trap cleanup EXIT

docker compose -p cpp-data-protection-core up --build --abort-on-container-exit --exit-code-from demo-runner demo-runner
docker compose -p cpp-data-protection-core down -v --remove-orphans
