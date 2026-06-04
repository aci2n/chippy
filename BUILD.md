# Building Chippy

Chippy is a single binary: local ledger commands and `chippy serve` (HTTP API + web UI).

```sh
./autogen.sh
./configure
make
```

Binary: `core/chippy` (or `make install` → `$prefix/bin/chippy`).

## Quick usage

```sh
# one-time setup
./core/chippy init --dir /tmp/.chippy
./core/chippy keygen
./core/chippy mint-key add <mint_addr> --dir /tmp/.chippy

# run API + browser UI on one port
./core/chippy serve --dir /tmp/.chippy --listen 127.0.0.1:8080
# open http://127.0.0.1:8080/

# local ops (no server)
./core/chippy balance <addr> --dir /tmp/.chippy
./core/chippy keygen
./core/chippy sign-mint <mint_secret> <mint_addr> <to> <amount>
```

Optional env for `serve`:

| Variable | Purpose | Default |
|----------|---------|---------|
| `CHIPPY_LISTEN` | Bind address `HOST:PORT` | `127.0.0.1:8080` |

## Tests

```sh
make check
```

Runs unit tests, CLI integration, and `chippy serve` HTTP smoke test.

## Layout

| Path | Role |
|------|------|
| `core/src/main.c` | Dispatches `serve` vs local commands |
| `core/src/cmd_local.c` | init, keygen, sign-*, mint, transfer, balance, validate |
| `core/src/cmd_serve.c` | `serve` — combined HTTP server |
| `core/src/http_server.c` | API routes + static files |
| `core/src/http_api.c` | JSON REST handlers |
| `core/static/` | HTML, CSS, JS (same-origin API) |
| `CHIPPY_LIB_SRC` in `core/Makefile.am` | Ledger engine (linked into `chippy` / `test_core`) |

## HTTP API (`chippy serve`)

| Method | Path | Body |
|--------|------|------|
| GET | `/health` | — |
| GET | `/api/v1/balance/:address` | — |
| GET | `/api/v1/chain/validate` | — |
| POST | `/api/v1/mint` | `{"to","amount","sig"}` |
| POST | `/api/v1/transfer` | `{"from","to","amount","sig"}` |

Static UI: `/`, `/style.css`, `/app.js`, `/config.js` (empty API base = same server).

Mint secrets stay client-side; `config` in the data dir lists `authorized_mint_key=` lines.

## Prerequisites

- C compiler, GNU Autotools, `pkg-config`, **libsodium**

Debug logging (`LOG_DEBUG` to stderr) is enabled by default. Disable with `./configure --disable-debug`.

## Generated files

Do not commit: `configure`, `Makefile`, `config.status`, `*.o`, `core/chippy`, etc. (see `.gitignore`).
