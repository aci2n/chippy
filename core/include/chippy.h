/*
 * Chippy core API.
 * Account-model ledger: blocks of signed txs, balances by replaying the chain.
 * Addresses are 64-char hex Ed25519 public keys; amounts are uint64_t units.
 * Most functions return 0 on success, -1 on error.
 */
#ifndef CHIPPY_H
#define CHIPPY_H

#include <stddef.h>
#include <stdint.h>

/* hex string lengths (no null counted) */
#define CHIPPY_HEX_HASH_LEN 64   /* sha256 digest */
#define CHIPPY_HEX_ADDR_LEN 64   /* 32-byte public key */
#define CHIPPY_HEX_SIG_LEN 128   /* 64-byte ed25519 signature */
#define CHIPPY_HEX_PUB_LEN 64
#define CHIPPY_HEX_SEC_LEN 128     /* 64-byte secret key */

#define CHIPPY_MAX_PATH 512
#define CHIPPY_DEFAULT_DIR ".chippy"

typedef enum {
  CHIPPY_TX_MINT = 0,      /* issue units; signed by mint authority */
  CHIPPY_TX_TRANSFER = 1,  /* move units; signed by from */
} chippy_tx_type;

typedef struct {
  chippy_tx_type type;
  char from[CHIPPY_HEX_ADDR_LEN + 1]; /* empty for mint */
  char to[CHIPPY_HEX_ADDR_LEN + 1];
  uint64_t amount;
  char sig[CHIPPY_HEX_SIG_LEN + 1];   /* hex-encoded detached signature */
} chippy_tx;

typedef struct {
  uint64_t index;
  uint64_t timestamp;
  char prev_hash[CHIPPY_HEX_HASH_LEN + 1];
  char hash[CHIPPY_HEX_HASH_LEN + 1];
  chippy_tx *txs;       /* heap array; owned by chain after append */
  size_t tx_count;
} chippy_block;

typedef struct {
  chippy_block *blocks;
  size_t block_count;
  size_t block_cap;
  char mint_pubkey[CHIPPY_HEX_PUB_LEN + 1]; /* from config; required for validate */
} chippy_chain;

/* --- util --- */

/* encode bytes to lowercase hex; hex_cap must be at least bin_len*2+1 */
int chippy_hex_encode(const unsigned char *bin, size_t bin_len, char *hex, size_t hex_cap);
/* decode hex to bytes; returns byte count or -1 */
int chippy_hex_decode(const char *hex, unsigned char *bin, size_t bin_cap);
/* 1 if strings equal, 0 otherwise (null-safe) */
int chippy_str_eq(const char *a, const char *b);

/* --- crypto (libsodium ed25519 + sha256) --- */

/* generate ed25519 keypair into pk/sk buffers */
int chippy_keypair_generate(unsigned char *pk, unsigned char *sk);
/* 32-byte pubkey -> address hex (same as pubkey hex) */
int chippy_pubkey_to_address(const unsigned char *pk, char *addr_hex);
/* address hex -> 32-byte pubkey */
int chippy_address_to_pubkey(const char *addr_hex, unsigned char *pk);
/* detached sign msg with sk; sig is crypto_sign_BYTES */
int chippy_sign(const unsigned char *sk, const unsigned char *msg, size_t msg_len,
                unsigned char *sig);
/* verify detached sig against address hex */
int chippy_verify(const char *addr_hex, const unsigned char *msg, size_t msg_len,
                  const unsigned char *sig);
/* sha256(data) as lowercase hex into out_hex */
int chippy_sha256_hex(const unsigned char *data, size_t data_len, char *out_hex);

/* --- tx --- */

/* build canonical sign bytes (newline-separated fields); writes length to out_len */
int chippy_tx_sign_payload(const chippy_tx *tx, unsigned char *buf, size_t buf_cap,
                           size_t *out_len);
/* fill tx->sig; tx must have type/fields set (mint or transfer) */
int chippy_tx_sign(chippy_tx *tx, const unsigned char *sk);
/* check sig against mint_pubkey (mint) or tx->from (transfer) */
int chippy_tx_verify(const chippy_tx *tx, const char *mint_pubkey);
/* parse chain-file tx body: "mint - to amt sig" or "transfer from to amt sig" */
int chippy_tx_parse_line(const char *line, chippy_tx *tx);
/* format tx as a chain-file tx line (without leading "tx ") */
int chippy_tx_format_line(const chippy_tx *tx, char *line, size_t line_cap);

/* --- chain --- */

/* zero chain; does not free prior blocks if reusing struct */
void chippy_chain_init(chippy_chain *chain);
/* free all blocks and txs */
void chippy_chain_free(chippy_chain *chain);
/* sha256 of canonical block serialization -> hash_out hex */
int chippy_block_compute_hash(const chippy_block *block, char *hash_out);
/*
 * full validation: hash links, stored hashes, sigs, mint authority, non-negative balances.
 * chain must have mint_pubkey set.
 */
int chippy_chain_validate(const chippy_chain *chain);
/* replay chain and return balance for addr_hex */
int chippy_chain_balance(const chippy_chain *chain, const char *addr_hex, uint64_t *balance_out);
/*
 * append block after validating chain+block in a replay copy.
 * on success, takes ownership of block->txs (sets them to null).
 */
int chippy_chain_append_block(chippy_chain *chain, chippy_block *block);

/* --- data directory lock (dir/lock, flock) --- */

/*
 * exclusive lock for one .chippy tree. re-entrant for the same dir in one process.
 * hold across load + append. returns -1 if a different dir is already locked.
 */
int chippy_dir_lock(const char *dir);
int chippy_dir_trylock(const char *dir);
int chippy_dir_unlock(const char *dir);

/* --- storage (.chippy/ data directory) --- */

/*
 * create dir: config (mint_pubkey) + genesis block.
 * generates a new mint keypair; only the pubkey is stored under dir.
 * optional out buffers receive mint address and secret hex (save secret off-server).
 * fails if dir already initialized.
 */
int chippy_storage_init(const char *dir, char *mint_address_out, char *mint_secret_out);
/* read mint_pubkey= from config */
int chippy_storage_load_config(const char *dir, char *mint_pubkey_out);
/* load config + parse chain file into chain (caller should chippy_chain_init first) */
int chippy_storage_load_chain(const char *dir, chippy_chain *chain);
/* append one block record to chain file */
int chippy_storage_append_block(const char *dir, const chippy_block *block);
/* read mint pubkey from config (same as load_config) */
int chippy_storage_mint_pubkey(const char *dir, char *pub_out);

#endif /* CHIPPY_H */
