#!/bin/sh
# integration test for backend REST API (mint via CLI only)
set -e

BACKEND="${CHIPPY_BACKEND:-./backend}"
CHIPPY="${CHIPPY:-../core/chippy}"
DIR=$(mktemp -d "${TMPDIR:-/tmp}/chippy-backend-XXXXXX")
PORT=${CHIPPY_TEST_PORT:-18080}
BASE="http://127.0.0.1:${PORT}"

trap 'kill $PID 2>/dev/null; rm -rf "$DIR"' EXIT

"$CHIPPY" init --dir "$DIR"

ALICE_KEYS=$("$CHIPPY" keygen)
ALICE=$(echo "$ALICE_KEYS" | sed -n 's/^address=//p')
ALICE_SK=$(echo "$ALICE_KEYS" | sed -n 's/^secret=//p')
BOB=$("$CHIPPY" keygen | sed -n 's/^address=//p')

"$CHIPPY" mint "$ALICE" 1000 --dir "$DIR"

"$BACKEND" --dir "$DIR" --listen "127.0.0.1:${PORT}" &
PID=$!
sleep 0.3

curl -sf "$BASE/health" | grep -q '"ok":true'

test "$(curl -sf "$BASE/api/v1/balance/$ALICE" | sed -n 's/.*"balance":\([0-9]*\).*/\1/p')" = "1000"

SIG=$("$CHIPPY" sign-transfer "$ALICE_SK" "$ALICE" "$BOB" 250)

curl -sf -X POST "$BASE/api/v1/transfer" \
  -H "Content-Type: application/json" \
  -d "{\"from\":\"$ALICE\",\"to\":\"$BOB\",\"amount\":250,\"sig\":\"$SIG\"}"

test "$(curl -sf "$BASE/api/v1/balance/$ALICE" | sed -n 's/.*"balance":\([0-9]*\).*/\1/p')" = "750"
test "$(curl -sf "$BASE/api/v1/balance/$BOB" | sed -n 's/.*"balance":\([0-9]*\).*/\1/p')" = "250"

curl -sf "$BASE/api/v1/chain/validate" | grep -q '"ok":true'

echo "ok backend integration"
