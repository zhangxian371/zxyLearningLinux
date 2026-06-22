#!/usr/bin/env bash
# Day 3 demo — exercise copy (small / empty / large / missing source) and the
# dup2 redirect demo. Run after `cmake --build build`.

set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "${HERE}/.." && pwd)"
BUILD="${ROOT}/build"

if [[ ! -x "${BUILD}/copy" || ! -x "${BUILD}/dup_demo" ]]; then
  echo "Build first:  cmake -S \"${ROOT}\" -B \"${BUILD}\" && cmake --build \"${BUILD}\"" >&2
  exit 1
fi

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

pass() { printf "  \033[32m✓\033[0m %s\n" "$1"; }
fail() { printf "  \033[31m✗\033[0m %s\n" "$1"; exit 1; }

echo "==> 1. copy a small file"
printf 'hello fd world\n' > "${WORK}/src.txt"
"${BUILD}/copy" "${WORK}/src.txt" "${WORK}/dst.txt" >/dev/null
diff -q "${WORK}/src.txt" "${WORK}/dst.txt" >/dev/null && pass "byte-identical" || fail "content differs"

echo "==> 2. copy an empty file"
: > "${WORK}/empty.txt"
"${BUILD}/copy" "${WORK}/empty.txt" "${WORK}/empty_copy.txt" >/dev/null
[[ -s "${WORK}/empty_copy.txt" ]] && fail "empty file became non-empty" || pass "empty stays empty"

echo "==> 3. copy a 10 MiB random file and verify sha256"
dd if=/dev/urandom of="${WORK}/big.bin" bs=1M count=10 status=none
SRC_SHA=$(sha256sum "${WORK}/big.bin" | awk '{print $1}')
"${BUILD}/copy" "${WORK}/big.bin" "${WORK}/big_copy.bin" >/dev/null
DST_SHA=$(sha256sum "${WORK}/big_copy.bin" | awk '{print $1}')
[[ "${SRC_SHA}" == "${DST_SHA}" ]] && pass "sha256 match (${SRC_SHA:0:12}…)" \
                                  || fail "sha256 mismatch: ${SRC_SHA} vs ${DST_SHA}"

echo "==> 4. copy a non-existent source (expect exit 1 + strerror message)"
if "${BUILD}/copy" "${WORK}/does_not_exist" "${WORK}/junk" >/dev/null 2>"${WORK}/err.log"; then
  fail "copy of missing source should have failed"
fi
grep -q "No such file or directory" "${WORK}/err.log" \
  && pass "got ENOENT message: $(tr -d '\n' < "${WORK}/err.log")" \
  || fail "missing ENOENT message: $(cat "${WORK}/err.log")"

echo "==> 5. dup2 redirect demo"
"${BUILD}/dup_demo" "${WORK}/redirect_out.txt" > "${WORK}/terminal_seen.txt"
printf '[before dup2] stdout is fd 1; this line goes to the terminal\n' > "${WORK}/expected_terminal.txt"
printf '[after  dup2] this line goes to %s, not the terminal\n' "${WORK}/redirect_out.txt" > "${WORK}/expected_file.txt"
diff -q "${WORK}/terminal_seen.txt" "${WORK}/expected_terminal.txt" >/dev/null \
  && pass "terminal saw only the pre-dup2 line" \
  || { fail "terminal saw wrong content"; cat "${WORK}/terminal_seen.txt"; }
diff -q "${WORK}/redirect_out.txt" "${WORK}/expected_file.txt" >/dev/null \
  && pass "file received only the post-dup2 line" \
  || { fail "file content wrong"; cat "${WORK}/redirect_out.txt"; }

echo
echo "All Day 3 demos passed."
