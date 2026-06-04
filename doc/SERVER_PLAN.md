# Chippy server plan (resume notes)

> **Obsolete.** Describes pre–PR #5 server wallets (`.sec` on disk) and a separate `backend/` binary. Current code: single `chippy serve`, client-side signing, `config` stores only `authorized_mint_key=` (public addresses).

Personal ledger toy — not production security. State lives in a single `.chippy/` tree on disk.

## Decision: link the engine, do not shell to CLI

| Approach | Verdict |
|----------|---------|
| **Link `libchippy` / engine objects** | **Yes** — same code as tests and CLI; use `chippy_dir_lock`; low latency |
| **Fork `chippy` per HTTP request** | Spike only — slow, stderr parsing, still needs flock |

```text
web/ (static UI)  →  server/ (HTTP)  →  libchippy.a + ops
dev               →  core/chippy CLI  →  same engine
```

## Repo layout (target)

```text
chippy/
  core/           # engine + CLI today
    include/chippy.h
    src/          # util crypto tx chain lock storage [ops.c later]
    src/main.c    # CLI only
  server/         # NEW: chippy-server binary
  web/            # static frontend → REST
  doc/            # this file
```

Root `Makefile.am`: `SUBDIRS = core server web`

## Process model

- **One server process**, one `--data-dir` (e.g. `/var/lib/chippy/.chippy`).
- **Single writer** at a time: `chippy_dir_lock` / `chippy_dir_unlock` (already in core; `flock` on `<dir>/lock`).
- Server may also use a **pthread mutex** so multiple HTTP threads do not interleave handler logic; file lock still required for CLI + server on same dir.
- **No** in-memory chain that diverges from disk — always `chippy_storage_load_chain` → mutate → `chippy_storage_append_block` for writes.

## Refactor before / with server

1. **`core/Makefile.am`**: build `noinst_LIBRARIES = libchippy.a` from `CHIPPY_ENGINE_SRC` (no `main.c`).
2. **`core/src/ops.c`** (new): move orchestration from `main.c`:
   - sign tx (`make_signed_tx`)
   - append block (load tip, hash, `storage_append_block`, `chain_append_block`)
   - high-level: `chippy_op_init`, `chippy_op_wallet_new`, `chippy_op_mint`, `chippy_op_send`, `chippy_op_balance`, `chippy_op_validate`
   - each op: `chippy_dir_lock` … work … `chippy_dir_unlock`
3. **`main.c`**: thin argv dispatch → `chippy_op_*`.
4. **`server/`**: HTTP layer → `chippy_op_*` only.

## REST API sketch (v1)

Base: `/api/v1` — data dir is server config, not per-request.

| Method | Path | Notes |
|--------|------|-------|
| GET | `/health` | ok |
| GET | `/balance/{address}` | `chippy_op_balance` |
| GET | `/chain/validate` | `chippy_op_validate` |
| POST | `/wallets` | body `{"name":"alice"}` |
| POST | `/mint` | `{"to","amount"}` — **admin only** |
| POST | `/transfer` | `{"wallet","to","amount"}` — server loads `wallets/<name>.sec` |

JSON errors on failure; HTTP 409 for validation / insufficient balance.

### Auth (v1 pragmatic)

- **Public read**: balance, validate (OK for toy).
- **Mint**: API key header or bind to localhost only.
- **Transfer**: server-side signing from `.chippy/wallets/` (browser never sees `.sec`). Same trust model as shared-machine CLI.

### Auth (v2 optional)

- Client signs in browser (WASM); server only accepts pre-signed tx + append.

## HTTP stack (C)

- **`chippy-server`** = `libchippy.a` + thin HTTP (e.g. civetweb or libmicrohttpd).
- Avoid CGI/fork-per-request.
- Optional: serve `web/` static files from same process.

## Lock API (implemented)

```c
int chippy_dir_lock(const char *dir);      /* blocks */
int chippy_dir_trylock(const char *dir);   /* busy → -1 */
int chippy_dir_unlock(const char *dir);    /* re-entrant same dir */
```

CLI already locks for full commands. Server must lock around every handler that touches storage.

**Do not** lock only inside individual `storage_*` calls without holding lock across load+append (gap between functions).

## Concurrency alternatives (why not X)

| Idea | Why skipped for now |
|------|---------------------|
| pthread mutex only | Does not stop CLI + server two processes |
| SQLite / LMDB | Model rewrite; overkill |
| Immutable tip + CAS | Too much design for learning project |
| CLI subprocess | See top — no |

Stick with **text chain + flock** until scale demands a single-writer service or embedded DB.

## Suggested build order

1. `libchippy.a` in `core/`
2. `ops.c` + slim `main.c`
3. `server/` minimal: GET balance, POST transfer, lock wrapper
4. `tests/test_api.sh` (curl) or extend `make check`
5. `web/` static UI calling API

## Do not

- Two writers without `chippy_dir_lock`
- Append without full-chain validation (`chippy_chain_append_block`)
- Public unauthenticated `mint` on the internet
- Separate RAM ledger out of sync with `chain` file

## Related docs

- `BUILD.md` — autotools, `make check`
- `RESUME_PROMPT.md` — original product spec
- `core/include/chippy.h` — API reference
