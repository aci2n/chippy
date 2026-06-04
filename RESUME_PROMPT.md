# Chippy — resume prompt

Copy this file into the new repo root (or paste into Cursor) when starting fresh.

---

## Goal

Build **Chippy**, a very simple blockchain-style cryptocurrency for **personal use only** (betting between friends, IOUs — **not real money**).

Long term: a **web frontend** to send and receive. For now, focus on the **C engine** and a minimal **CLI**.

## Non-goals

- No mining, no proof-of-work
- Not production-grade security or economics
- Keep it **simple** — this is a **learning project**

## Design constraints (fixed)

| Topic | Choice |
|-------|--------|
| Engine language | **C only** |
| Repo layout | **Monorepo**: `core/` (engine + CLI), `web/` (placeholder for later), room for more |
| Build system | **GNU Autotools** (`autoreconf`, `configure`, `Makefile.am`) |
| Crypto library | **libsodium** (Ed25519 sign/verify, SHA256 for block hashes) |
| Data directory | **`.chippy/`** (default) |
| Public API prefix | `CHIPPY_` |
| CLI binary name | `chippy` |

## Naming

- Project: **Chippy** (playful, poker-chips vibe)
- GitHub repo name can be changed later; local folder name does not have to match

## Core model (keep simple)

- **Account balances** (not UTXO): scan the chain and sum credits/debits per address
- **Address** = hex-encoded 32-byte Ed25519 public key (64 hex chars)
- **Amounts** = `uint64_t` integer units (no floats)

### Transaction types

1. **Mint** — creates balance for `to`. Must be signed by the **mint authority** key (privileged issuer). No sender.
2. **Transfer** — moves `amount` from `from` to `to`. Signed by `from`. Reject if insufficient balance.

### Blocks

- Linked list: each block has `index`, `timestamp`, `prev_hash`, `txs[]`, `hash`
- Genesis block: index 0, `prev_hash` all zeros, no transactions
- Block hash: SHA256 over canonical serialization (index, time, prev_hash, signed tx payloads)
- **No mining**: append a new block whenever the CLI (or later the web layer) submits valid tx(s)

### Persistence (text, easy to debug)

Under `.chippy/`:

```
config          # mint_pubkey=<64 hex chars>
mint.pub / mint.sec
wallets/<name>.pub / wallets/<name>.sec
chain           # line-oriented block + tx format
```

Example chain lines (illustrative):

```
block 0
time 1717500000
prev 0000...0000
hash abcd...abcd

block 1
time 1717500100
prev abcd...abcd
tx mint - <to_addr> <amount> <sig_hex>
hash ...
```

Mint tx line uses `-` as placeholder for empty `from`.

### Validation rules

1. Hash chain integrity (`prev_hash` matches previous block)
2. Each block’s stored `hash` matches recomputed hash
3. Every tx has a valid Ed25519 signature on its sign payload
4. Mint txs: signer must equal configured mint pubkey
5. Transfer txs: signer is `from`, balance never goes negative

## CLI commands (first milestone)

```
chippy init [--dir .chippy]     # create mint key, config, genesis block
chippy wallet new <name>        # new wallet under wallets/
chippy mint <to_addr> <amount>  # mint authority only; one tx per new block is fine
chippy send <wallet> <to> <amount>
chippy balance <addr>
chippy validate                 # full chain check
```

Initialize libsodium in `main` before any crypto (`sodium_init()`).

## Build layout (target tree)

```
chippy/                    # repo root (name on GitHub can be chippy)
  configure.ac
  Makefile.am              # SUBDIRS = core web
  autogen.sh
  m4/
  core/
    Makefile.am
    include/chippy.h
    src/util.c crypto.c tx.c chain.c storage.c
    src/main.c
  web/
    Makefile.am            # placeholder: echo "not implemented"
```

`autogen.sh` → `./configure` → `make` → `core/chippy`

`.gitignore`: autotools artifacts, `core/chippy`, `.chippy/`, `*.key`

## Web (later)

- `web/` stays a stub until core is solid
- Eventually: HTTP API or static frontend calling the C library or CLI
- Operations needed from the UI: balance, send, (mint only if admin)

## Suggested implementation order

1. Autotools skeleton + libsodium `PKG_CHECK_MODULES`
2. `chippy.h` types + hex util + keygen/sign/verify
3. Tx sign payload + verify (mint vs transfer)
4. Block hash + chain validate + balance
5. Load/save `.chippy/` files
6. CLI wired end-to-end; manual test: init → mint → send → balance → validate

## Notes from earlier attempt (avoid repeating mistakes)

- Use **double-quoted** C strings (`"mint"`), not single quotes
- `chain_grow()` for dynamic block array must live in **one** compilation unit (e.g. `chain.c`), or load/save must be in the same file — do not call `static` helpers across files
- `storage.c` needs `#include <time.h>` if it calls `time()`
- Fix mint tx parse: line format `mint - <to> <amount> <sig>` (sscanf-friendly)
- `chippy_chain_append_block`: when appending, replay **entire existing chain** first for balance checks, then apply new txs
