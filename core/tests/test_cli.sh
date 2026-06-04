#!/bin/sh
# integration test: drive the chippy CLI end-to-end
set -e

CHIPPY="${CHIPPY:-./chippy}"
DIR=$(mktemp -d "${TMPDIR:-/tmp}/chippy-cli-XXXXXX")
trap 'rm -rf "$DIR"' EXIT

"$CHIPPY" init --dir "$DIR"
ALICE=$("$CHIPPY" wallet new alice --dir "$DIR")
BOB=$("$CHIPPY" wallet new bob --dir "$DIR")
"$CHIPPY" mint "$ALICE" 1000 --dir "$DIR"
test "$("$CHIPPY" balance "$ALICE" --dir "$DIR")" = "1000"
"$CHIPPY" send alice "$BOB" 250 --dir "$DIR"
test "$("$CHIPPY" balance "$ALICE" --dir "$DIR")" = "750"
test "$("$CHIPPY" balance "$BOB" --dir "$DIR")" = "250"
"$CHIPPY" validate --dir "$DIR"

echo "ok cli integration"
