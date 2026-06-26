#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${DPC_BUILD_DIR:-${ROOT}/build}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-RelWithDebInfo}"

GENERATOR_ARGS=()
if [[ -n "${CMAKE_GENERATOR:-}" ]]; then
  GENERATOR_ARGS=(-G "${CMAKE_GENERATOR}")
fi

EXTRA_ARGS=()
if [[ -n "${CMAKE_ARGS:-}" ]]; then
  read -r -a EXTRA_ARGS <<< "${CMAKE_ARGS}"
fi

cmake -S "${ROOT}" -B "${BUILD_DIR}" "${GENERATOR_ARGS[@]}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" "${EXTRA_ARGS[@]}"
cmake --build "${BUILD_DIR}" -j"$(nproc)"
