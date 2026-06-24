#!/usr/bin/env bash
set -euo pipefail

HOST="${1:-127.0.0.1}"
PORT="${2:-8080}"

python3 - "$HOST" "$PORT" <<'PY'
import socket
import struct
import sys

host = sys.argv[1]
port = int(sys.argv[2])
s = socket.create_connection((host, port))
s.sendall(struct.pack("!I", 2 * 1024 * 1024))
s.close()
print("sent oversized frame length")
PY

