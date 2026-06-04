#!/bin/sh
# smoke test: web server serves HTML and injects CHIPPY_API_URL into config.js
set -e

WEB="${1:?web binary}"
PORT=$(python3 -c 'import socket; s=socket.socket(); s.bind(("127.0.0.1",0)); print(s.getsockname()[1]); s.close()')
BASE="http://127.0.0.1:${PORT}"

trap 'kill $PID 2>/dev/null' EXIT

CHIPPY_API_URL="http://127.0.0.1:18080" CHIPPY_LISTEN="127.0.0.1:${PORT}" "$WEB" &
PID=$!
sleep 0.3

curl -sf "$BASE/" | grep -q '<title>Chippy</title>'
curl -sf "$BASE/config.js" | grep -q 'window.CHIPPY_API_URL="http://127.0.0.1:18080"'
curl -sf "$BASE/style.css" | grep -q 'font-family'
curl -sf "$BASE/app.js" | grep -q 'apiFetch'

echo "ok web smoke"
