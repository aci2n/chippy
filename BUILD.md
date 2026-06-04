# Building Chippy

Chippy uses GNU Autotools at the **repository root** and builds the `chippy` CLI from `core/` and the `httpapi` REST backend. The `web/` directory is a stub for now.

You only need the three usual steps: generate `configure`, run `./configure`, then `make`.

## Prerequisites

- C compiler (`gcc` or `clang`)
- GNU Autotools: `autoconf`, `automake`, `libtool` (for `./autogen.sh`)
- `pkg-config` and **libsodium** (`brew install automake libsodium` on macOS)

## Quick start

From the repo root:

```sh
./autogen.sh
./configure
make
```

Binaries: `core/chippy` (CLI), `httpapi/httpapi` (HTTP API server).

To rebuild after pulling changes to `configure.ac` or any `Makefile.am`:

```sh
./autogen.sh    # only if autotools inputs changed
./configure     # only if configure.ac changed or you cleaned configure output
make
```

## The three phases

| Step | Command | What it does |
|------|---------|----------------|
| 1. Bootstrap | `./autogen.sh` | Runs `autoreconf -fi` to create `configure` and `Makefile.in` files |
| 2. Configure | `./configure` | Probes the host (compiler, libsodium) and writes `Makefile`s |
| 3. Build | `make` | Recurses into `core/`, `httpapi/`, and `web/` |

Day-to-day work is usually just `make` once `./configure` has been run.

## Tests

GNU convention: run the test suite with

```sh
make check
```

This builds `core/test_core` (unit tests against the engine) and runs `core/tests/test_cli.sh` (CLI integration). Tests use temporary directories under `/tmp`; they do not touch `.chippy/` in the repo.

To run only core tests:

```sh
make -C core check
```

## Checked-in build files (what each one is)

These are the files you edit. Everything else under this list’s “Generated” column should stay out of git (see `.gitignore`).

### Repo root

| File | Role |
|------|------|
| `autogen.sh` | Thin wrapper around `autoreconf -fi`. Regenerates `configure` and `Makefile.in`; `-i` installs helper scripts (`compile`, `install-sh`, …) if missing. |
| `configure.ac` | Autoconf input: package metadata, `AC_PROG_CC`, `PKG_CHECK_MODULES` for libsodium, and which `Makefile`s to emit. |
| `Makefile.am` | Automake input for the top level. Only lists `SUBDIRS = core web` so `make` recurses. |
| `m4/` | Directory for Autoconf macros (empty except `.gitkeep` today). |

### `core/`

| File | Role |
|------|------|
| `Makefile.am` | Defines the `chippy` program and `make check` targets (`test_core`, `tests/test_cli.sh`). |
| `tests/test_core.c` | Unit tests for hex, crypto, tx, chain, and storage. |
| `tests/test_cli.sh` | CLI integration test (init → mint → send → validate). |
| `include/chippy.h` | Ledger engine API (crypto, tx, chain, storage). |
| `include/chippy_ops.h` | High-level ops (init, mint, transfer, balance, validate, keygen). |
| `libchippy.a` | Static library: engine + ops (linked by `chippy` and tests). |
| `src/main.c` | CLI only; dispatches to `chippy_op_*`. |

### `httpapi/`

| File | Role |
|------|------|
| `Makefile.am` | Builds `httpapi` binary linked against `core/libchippy.a`. |
| `include/chippy_ops.h` (via `-I../core/include`) | Ledger operations. |
| `include/api.h`, `src/api.c` | REST route handlers (JSON). |
| `src/server.c` | TCP HTTP/1.1 server using **picohttpparser** (vendored). |
| `vendor/picohttpparser.c` | Minimal HTTP request parser (MIT). |
| `tests/test_api.sh` | `curl` integration test (run via `make check`). |

Run the API server:

```sh
./httpapi/httpapi --dir /tmp/.chippy --listen 127.0.0.1:8080 --mint-token dev-secret
```

Optional: set `CHIPPY_MINT_TOKEN` instead of `--mint-token` to require `X-Chippy-Mint-Token` on `POST /api/v1/mint`.

### `web/`

| File | Role |
|------|------|
| `Makefile.am` | Placeholder: `make` prints `web: not implemented`. |

## Generated files (do not commit)

After `./autogen.sh`:

- `configure` — portable shell script produced from `configure.ac`
- `Makefile.in` (root, `core/`, `web/`) — templates for Automake
- `autom4te.cache/`, `aclocal.m4` — autoreconf scratch
- Helper scripts at repo root: `compile`, `install-sh`, `config.guess`, `depcomp`, … (installed by `autoreconf -i`)

After `./configure`:

- `Makefile` (root, `core/`, `web/`) — concrete build rules for this machine
- `config.status`, `config.log` — configure cache and log

After `make`:

- `core/chippy` — CLI binary
- `core/**/*.o`, `core/**/.deps/`, `.dirstamp` — compile artifacts

Configure also drops `confdefs.h` at the root during the probe; it is ignored.

The generated `Makefile` calls the helper scripts (e.g. `compile`); their logic is not inlined into `Makefile`. That is normal for Automake. They are safe to delete; run `./autogen.sh` again before the next `./configure` if they are gone.

## Why Autotools is at the root, not in `core/`

The repo is a small **monorepo**: `core/` (C engine today) and `web/` (later). One `./configure` checks shared dependencies once (libsodium) and generates Makefiles for all subdirs. Product name and version live in root `configure.ac` as package `chippy`.

## Useful commands

```sh
# build only the CLI (from repo root)
make -C core

# clean build products (keeps configure / Makefile)
make clean

# remove generated and ignored artifacts (configure, Makefile, binary, aux scripts, …)
git clean -xfd

# full rebuild from a clean tree
./autogen.sh && ./configure && make
```

## Configure options

No custom switches are defined yet. Standard Autotools variables work, for example:

```sh
./configure CC=clang
make install   # installs to $(prefix)/bin if you add install targets later
```

## Troubleshooting

| Problem | Likely fix |
|---------|------------|
| `aclocal: not found` / `automake: not found` | Install Autotools (`brew install automake`) |
| `Package libsodium not found` | Install libsodium dev package; ensure `pkg-config` sees it |
| `./configure: No such file` | Run `./autogen.sh` first |
| `make`: missing `compile` or `install-sh` | Run `./autogen.sh` (with `-i`) to restore aux files |
| Stale build after macro changes | Re-run `./autogen.sh`, then `./configure`, then `make clean && make` |
