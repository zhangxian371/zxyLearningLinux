#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-8080}"
WORKERS="${2:-4}"

cmake -S "$ROOT" -B "$ROOT/build"
cmake --build "$ROOT/build" -j
exec "$ROOT/build/chat_server" "$PORT" "$WORKERS"

