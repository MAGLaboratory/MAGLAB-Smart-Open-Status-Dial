#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SIM_DIR="$REPO_ROOT/tools/host-sim"
BUILD_DIR="$REPO_ROOT/build/host-sim"

mkdir -p "$BUILD_DIR"

cmake -S "$SIM_DIR" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR"

if [[ ${1:-} == "run" ]]; then
  shift
  SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-} "$BUILD_DIR/m5dial_host_sim" "$@"
fi
