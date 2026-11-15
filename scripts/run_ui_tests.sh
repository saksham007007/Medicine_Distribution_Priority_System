#!/usr/bin/env bash
# Real-browser UI tests: bundles the React app (esbuild + test shims), serves it from the C++ server, drives it with headless Chromium.
# Needs: node, esbuild + react + react-dom + playwright (+ Chromium). Sandbox: NODE_PATH=/opt/npm-tools/node_modules
# (The shipped frontend is built with Vite and the real libraries; see docs/TESTING.md for what this test does and does not prove.)
set -u; cd "$(dirname "$0")/.."
OUT=${OUT:-build}; PORT=${UI_PORT:-8098}; mkdir -p "$OUT"
INC=${SQLITE_INCLUDE:+-I$SQLITE_INCLUDE}; LIB=${SQLITE_LIB:--lsqlite3}
g++ -std=c++17 -O2 -Wall -Wextra $INC -pthread backend/main.cpp $LIB -o "$OUT/medicine_server" || exit 1
node tests/ui/build.cjs 2>&1 | grep -v -E "WARNING|import.meta|iife|frontend/src/api.js|~~~|^\s*$|You need|│|╵" 
rm -f "$OUT/ui.db" "$OUT/ui.db-wal" "$OUT/ui.db-shm"
"$OUT/medicine_server" --db "$OUT/ui.db" --port "$PORT" --static tests/ui/build > "$OUT/ui_server.log" 2>&1 & SERVER=$!
for i in $(seq 1 60); do curl -s "http://127.0.0.1:$PORT/api/health" > /dev/null 2>&1 && break; sleep 0.5; done
node tests/ui/ui_test.cjs "http://127.0.0.1:$PORT"; status=$?
kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null
exit $status
