#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-8080}"

cmake -S "$ROOT" -B "$ROOT/build"
cmake --build "$ROOT/build" -j
"$ROOT/build/chat_bench" 100 10 "$PORT"

