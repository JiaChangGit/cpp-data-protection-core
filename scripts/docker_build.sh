#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT}"

docker build --target dev -t cpp-data-protection-core:dev .
docker build --target test -t cpp-data-protection-core:test .
docker build --target runtime -t cpp-data-protection-core:runtime .
