#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_DIR="$REPO_ROOT/apps/m5dial-timer"

if [[ ! -d "$APP_DIR" ]]; then
  echo "Unable to locate m5dial-timer app directory" >&2
  exit 1
fi

ESP_IDF_DIR="${ESP_IDF:-$HOME/esp-idf}"
if [[ ! -d "$ESP_IDF_DIR" ]]; then
  echo "ESP-IDF not found at $ESP_IDF_DIR" >&2
  echo "Set ESP_IDF env var or install to ~/esp-idf" >&2
  exit 1
fi

# Activate ESP-IDF environment quietly
# shellcheck disable=SC1090
source "$ESP_IDF_DIR/export.sh" >/dev/null

PORT=${1:-}
if [[ -z "$PORT" ]]; then
  # try to auto-detect a USB serial port
  CANDIDATES=(/dev/ttyACM* /dev/ttyUSB*)
  for candidate in "${CANDIDATES[@]}"; do
    if [[ -e "$candidate" ]]; then
      PORT="$candidate"
      break
    fi
  done
  if [[ -z "$PORT" ]]; then
    echo "No serial port provided or auto-detected." >&2
    echo "Usage: $0 [PORT]" >&2
    exit 1
  fi
fi

idf.py -C "$APP_DIR" -p "$PORT" flash monitor
