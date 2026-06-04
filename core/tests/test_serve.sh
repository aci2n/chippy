#!/bin/sh
# integration test: chippy serve (API + static UI on one port)
set -e

CHIPPY="${CHIPPY:-./chippy}"
DIR=$(mktemp -d "${TMPDIR:-/tmp}/chippy-serve-XXXXXX")
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')
BASE="http://127.0.0.1:${PORT}"

trap 'kill $PID 2>/dev/null; rm -rf "$DIR"' EXIT

INIT=$("$CHIPPY" init --dir "$DIR")
MINT_ADDR=$(echo "$INIT" | sed -n 's/^mint_address=//p')
MINT_SK=$(echo "$INIT" | sed -n 's/^mint_secret=//p')

ALICE_KEYS=$("$CHIPPY" keygen)
ALICE=$(echo "$ALICE_KEYS" | sed -n 's/^address=//p')
ALICE_SK=$(echo "$ALICE_KEYS" | sed -n 's/^secret=//p')
BOB=$("$CHIPPY" keygen | sed -n 's/^address=//p')

"$CHIPPY" serve --dir "$DIR" --listen "127.0.0.1:${PORT}" &
PID=$!
sleep 0.3

curl -sf "$BASE/health" | grep -q '"ok":true'
curl -sf "$BASE/" | grep -q '<title>Chippy</title>'
curl -sf "$BASE/config.js" | grep -q 'window.CHIPPY_API_URL=""'

MINT_SIG=$("$CHIPPY" sign-mint "$MINT_SK" "$MINT_ADDR" "$ALICE" 1000)
curl -sf -X POST "$BASE/api/v1/mint" \
  -H "Content-Type: application/json" \
  -d "{\"to\":\"$ALICE\",\"amount\":1000,\"sig\":\"$MINT_SIG\"}"

test "$(curl -sf "$BASE/api/v1/balance/$ALICE" | sed -n 's/.*"balance":\([0-9]*\).*/\1/p')" = "1000"

SIG=$("$CHIPPY" sign-transfer "$ALICE_SK" "$ALICE" "$BOB" 250)
curl -sf -X POST "$BASE/api/v1/transfer" \
  -H "Content-Type: application/json" \
  -d "{\"from\":\"$ALICE\",\"to\":\"$BOB\",\"amount\":250,\"sig\":\"$SIG\"}"

test "$(curl -sf "$BASE/api/v1/balance/$ALICE" | sed -n 's/.*"balance":\([0-9]*\).*/\1/p')" = "750"
curl -sf "$BASE/api/v1/chain/validate" | grep -q '"ok":true'

echo "ok serve integration"
