#!/usr/bin/env bash
# Smoke test: build, launch briefly, assert a clean startup.
# Requires a display ($DISPLAY) — the app opens a real GLFW window.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

echo "== build =="
cmake -S "$ROOT" -B "$ROOT/build" -G Ninja >/dev/null
cmake --build "$ROOT/build" >/dev/null

echo "== run (3s) =="
timeout 3 "$ROOT/build/smallgine" >"$LOG" 2>&1 || true
cat "$LOG"

echo "== checks =="
grep -q "Engine initialized" "$LOG" || { echo "FAIL: engine did not initialize"; exit 1; }
if grep -qiE "error|fatal|incomplete|failed" "$LOG"; then
    echo "FAIL: errors present in startup log"
    exit 1
fi

echo "SMOKE PASS"
