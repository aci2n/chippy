# Chippy — resume on your PC

Use this file to pick up where the cloud agent left off. Paste into Cursor or read before coding.

**Repo:** `aci2n/chippy`  
**Active branch (unmerged refactor):** `cursor/unified-chippy-binary-c858`  
**Open PR:** [#5 — Single chippy binary: serve (API + UI) and local commands](https://github.com/aci2n/chippy/pull/5) (draft)

`master` still has the **three-binary** layout (`core/chippy`, `backend/backend`, `web/web`). PR #5 collapses everything into one `chippy` binary.

---

## What Chippy is

Personal ledger toy (not real money): account balances, Ed25519-signed txs, append-only chain on disk.

- **Mint** — signed by an authorized mint key (pubkeys in `config`)
- **Transfer** — signed by `from`
- Mint **secrets never on server**; CLI signs, server only appends verified txs

---

## Current architecture (PR #5 branch)

**One binary:** `core/chippy`

| Command | Role |
|---------|------|
| `chippy serve` | Single HTTP server: REST API **and** static web UI (same port, same origin) |
| `chippy init` | Create `.chippy/`, first `authorized_mint_key=`, print mint secret once |
| `chippy keygen` | Client keypair (no data dir) |
| `chippy sign-mint` / `sign-transfer` | Client signing (no data dir) |
| `chippy mint` / `transfer` | Append pre-signed tx to local chain file |
| `chippy balance` / `validate` | Local chain queries |
| `chippy mint-key add <addr>` | Append `authorized_mint_key=` to config |

**Dispatch:** all commands are peers in `g_commands[]` in `core/src/main.c`.

### Folder layout (after merge)

```
chippy/
├── autogen.sh, configure.ac, Makefile.am   # SUBDIRS = core only
├── BUILD.md
├── docs/
│   └── RESUME_SESSION.md    # this file
├── doc/
│   └── SERVER_PLAN.md       # outdated (pre-unified binary)
└── core/
    ├── chippy                 # build output
    ├── include/               # chippy.h, chippy_ops.h, http_*, cmd_*
    ├── src/
    │   ├── main.c             # command table + dispatch
    │   ├── cmd_local.c        # init, mint, transfer, …
    │   ├── cmd_serve.c        # serve
    │   ├── http_server.c      # API routes + static files
    │   ├── http_api.c         # /api/v1/*
    │   ├── json_util.c
    │   └── util, crypto, tx, chain, lock, storage, ops, mint_keys
    ├── static/                # index.html, app.js, style.css
    ├── vendor/picohttpparser.*
    └── tests/
        ├── test_core.c
        ├── test_cli.sh
        └── test_serve.sh
```

**Removed on PR branch:** `backend/`, `web/` as separate programs.

### Data directory (`.chippy/`)

```
config     # authorized_mint_key=<64 hex> (one or more lines)
chain      # blocks + txs
lock       # flock for single writer
```

Legacy `mint_pubkey=` is **not** read anymore.

---

## Build on your PC

**Needs:** gcc/clang, autoconf, automake, libtool, pkg-config, libsodium

```sh
git fetch origin
git checkout cursor/unified-chippy-binary-c858   # or master if #5 merged

./autogen.sh
./configure
make
make check
```

Binary: `core/chippy`

### Run

```sh
# setup once
./core/chippy init --dir /tmp/.chippy

# API + browser UI on one port
./core/chippy serve --dir /tmp/.chippy --listen 127.0.0.1:8080
# open http://127.0.0.1:8080/

# local ops (separate terminal)
./core/chippy keygen
./core/chippy sign-mint <mint_sec> <mint_addr> <to> <amount>
./core/chippy balance <addr> --dir /tmp/.chippy
```

Optional env for `serve`:

- `CHIPPY_LISTEN=HOST:PORT` — bind address (default `127.0.0.1:8080`)

UI calls `/api/v1/...` on the same host (`/config.js` sets empty API base = same origin).

---

## HTTP API (`chippy serve`)

| Method | Path |
|--------|------|
| GET | `/health` |
| GET | `/api/v1/balance/:address` |
| GET | `/api/v1/chain/validate` |
| POST | `/api/v1/mint` — `{"to","amount","sig"}` |
| POST | `/api/v1/transfer` — `{"from","to","amount","sig"}` |

Static: `/`, `/style.css`, `/app.js`, `/config.js`

---

## Design choices (session history)

1. **No server wallets** — users sign transfers client-side; server stores no `.sec` files.
2. **`libchippy.a`** — engine + `chippy_op_*` shared by CLI and HTTP layer.
3. **`authorized_mint_key=`** — multiple mint pubkeys in config; mint sig must match one of them.
4. **HTTP backend** (PR #3) → **frontend** (PR #4) → **unified binary** (PR #5).
5. **CLI = admin + signer** for dev; web UI submits pasted signatures (minimal JS, no libsodium in browser yet).

---

## Merged PRs on `master`

| PR | Summary |
|----|---------|
| #1 | Client-side signing; remove server wallets |
| #2 | `libchippy.a` + `chippy_op_*` |
| #3 | `backend/` HTTP API |
| #4 | `web/` static server + `CHIPPY_API_URL` |
| #5 | **Pending** — single `chippy serve` + local commands |

---

## Likely next steps on PC

1. Review and merge **PR #5** (mark ready → merge).
2. Delete stale branches if desired.
3. Update or archive `doc/SERVER_PLAN.md` and root `RESUME_PROMPT.md` (both outdated).
4. Optional: `make install`, GitHub release with single `chippy` artifact.
5. Optional: embed static files in binary; browser signing (WASM); admin HTTP for `mint-key add`.

---

## Quick sanity check

```sh
make check
# PASS: test_core, test_cli.sh, test_serve.sh
```

---

## Cursor hint

> Continue Chippy on branch `cursor/unified-chippy-binary-c858`. Read `docs/RESUME_SESSION.md`. Single binary `chippy` with `serve` (API+UI) and local subcommands. Help me merge PR #5 / \<your next task\>.
