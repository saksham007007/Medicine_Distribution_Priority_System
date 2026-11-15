#!/usr/bin/env bash
# Builds the backend, runs all test suites against a fresh database, and stops the server.
# Normal machines (sqlite3.h + libsqlite3 installed):   ./scripts/run_tests.sh
# Sandbox without sqlite3.h (test-only stub header):    SQLITE_INCLUDE=tests/sqlite_stub SQLITE_LIB=/usr/lib/x86_64-linux-gnu/libsqlite3.so.0 ./scripts/run_tests.sh
set -u
cd "$(dirname "$0")/.."
OUT=${OUT:-build}; mkdir -p "$OUT"; PORT=${PORT:-8099}
INC=${SQLITE_INCLUDE:+-I$SQLITE_INCLUDE}; LIB=${SQLITE_LIB:--lsqlite3}
status=0
SAN=""; OPT="-O2"; [ "${SANITIZE:-0}" = "1" ] && { SAN="-fsanitize=address,undefined -fno-omit-frame-pointer"; OPT="-O1 -g"; echo "(AddressSanitizer + UBSan build)"; }
echo "== Building"; g++ -std=c++17 $OPT $SAN -Wall -Wextra $INC -pthread backend/main.cpp $LIB -o "$OUT/medicine_server" || exit 1
g++ -std=c++17 -Wall -Wextra -pthread tests/unit_tests.cpp -o "$OUT/unit_tests" || exit 1
echo "== Unit tests (JSON, crypto vectors, validation, router)"; "$OUT/unit_tests" || status=1
rm -f "$OUT/test.db" "$OUT/test.db-wal" "$OUT/test.db-shm"
ASAN_OPTIONS=detect_leaks=0 "$OUT/medicine_server" --db "$OUT/test.db" --port "$PORT" > "$OUT/server.log" 2>&1 & SERVER=$!
for i in $(seq 1 60); do curl -s "http://127.0.0.1:$PORT/api/health" > /dev/null 2>&1 && break; sleep 0.5; done
echo "== Database tests"; python3 tests/test_database.py "$OUT/test.db" || status=1
echo "== API workflow tests"; python3 tests/test_api.py "http://127.0.0.1:$PORT" "$OUT/test.db" || status=1
kill $SERVER 2>/dev/null; wait $SERVER 2>/dev/null
[ "${SANITIZE:-0}" = "1" ] && { if grep -qE "ERROR: AddressSanitizer|runtime error" "$OUT/server.log"; then echo "FAIL: sanitizer reported errors:"; grep -E "ERROR: AddressSanitizer|runtime error" "$OUT/server.log" | head; status=1; else echo "PASS: no AddressSanitizer/UBSan findings during the full API run"; fi; }
echo "== CLI mode (original console menu on the new service layer)"
rm -f "$OUT/cli.db" "$OUT/cli.db-wal" "$OUT/cli.db-shm"
CLI_OUT=$(printf '1\n2\ny\n8\n0\n' | "$OUT/medicine_server" --db "$OUT/cli.db" --cli 2>&1)
echo "$CLI_OUT" | grep -q "Medicine distributed" && echo "$CLI_OUT" | grep -q "Distributed" && ! echo "$CLI_OUT" | grep -q "Error:" && echo "PASS: CLI shows queue, distributes the head request and lists the transaction" || { echo "FAIL: CLI workflow"; echo "$CLI_OUT" | tail -15; status=1; }
echo "== Idempotent seeding"; A=$("$OUT/medicine_server" --db "$OUT/test.db" --seed | grep -E "^(medicines|pharmacies|pharmacy_inventory|medicine_batches|medicine_alternatives|patients) " | tr '\n' ' ')
B=$("$OUT/medicine_server" --db "$OUT/test.db" --seed | grep -E "^(medicines|pharmacies|pharmacy_inventory|medicine_batches|medicine_alternatives|patients) " | tr '\n' ' ')
[ "$A" = "$B" ] && echo "PASS: running --seed twice changed no table counts ($B)" || { echo "FAIL: seeding is not idempotent"; status=1; }
[ $status -eq 0 ] && echo "ALL TEST SUITES PASSED" || echo "SOME TESTS FAILED"
exit $status
