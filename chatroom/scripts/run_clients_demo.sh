#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PORT="${1:-8080}"

cat <<EOF
Open three terminals and run:

  $ROOT/build/chat_client 127.0.0.1 $PORT

Then in each client:

  /register alice secret
  /login alice secret
  /auth <token-from-login-response>
  /join 1
  /send 1 hello

Use different names in each terminal.
EOF

