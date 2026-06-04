#!/bin/sh
# integration test: drive the chippy CLI end-to-end (client-side keys)
set -e

CHIPPY="${CHIPPY:-./chippy}"
DIR=$(mktemp -d "${TMPDIR:-/tmp}/chippy-cli-XXXXXX")
trap 'rm -rf "$DIR"' EXIT

INIT=$("$CHIPPY" init --dir "$DIR")
MINT_ADDR=$(echo "$INIT" | sed -n 's/^mint_address=//p')
MINT_SK=$(echo "$INIT" | sed -n 's/^mint_secret=//p')
ALICE_KEYS=$("$CHIPPY" keygen)
ALICE=$(echo "$ALICE_KEYS" | sed -n 's/^address=//p')
ALICE_SK=$(echo "$ALICE_KEYS" | sed -n 's/^secret=//p')
BOB=$("$CHIPPY" keygen | sed -n 's/^address=//p')
MINT_SIG=$("$CHIPPY" sign-mint "$MINT_SK" "$MINT_ADDR" "$ALICE" 1000)
"$CHIPPY" mint "$ALICE" 1000 "$MINT_SIG" --dir "$DIR"
test "$("$CHIPPY" balance "$ALICE" --dir "$DIR")" = "1000"
SIG=$("$CHIPPY" sign-transfer "$ALICE_SK" "$ALICE" "$BOB" 250)
"$CHIPPY" transfer "$ALICE" "$BOB" 250 "$SIG" --dir "$DIR"
test "$("$CHIPPY" balance "$ALICE" --dir "$DIR")" = "750"
test "$("$CHIPPY" balance "$BOB" --dir "$DIR")" = "250"
"$CHIPPY" validate --dir "$DIR"

echo "ok cli integration"
