#ifndef CHIPPY_H
#define CHIPPY_H

#include <stddef.h>
#include <stdint.h>

#define CHIPPY_HEX_HASH_LEN 64
#define CHIPPY_HEX_ADDR_LEN 64
#define CHIPPY_HEX_SIG_LEN 128
#define CHIPPY_HEX_PUB_LEN 64
#define CHIPPY_HEX_SEC_LEN 128

#define CHIPPY_MAX_PATH 512
#define CHIPPY_DEFAULT_DIR ".chippy"

typedef enum {
  CHIPPY_TX_MINT = 0,
  CHIPPY_TX_TRANSFER = 1,
} chippy_tx_type;

typedef struct {
  chippy_tx_type type;
  char from[CHIPPY_HEX_ADDR_LEN + 1];
  char to[CHIPPY_HEX_ADDR_LEN + 1];
  uint64_t amount;
  char sig[CHIPPY_HEX_SIG_LEN + 1];
} chippy_tx;

typedef struct {
  uint64_t index;
  uint64_t timestamp;
  char prev_hash[CHIPPY_HEX_HASH_LEN + 1];
  char hash[CHIPPY_HEX_HASH_LEN + 1];
  chippy_tx *txs;
  size_t tx_count;
} chippy_block;

typedef struct {
  chippy_block *blocks;
  size_t block_count;
  size_t block_cap;
  char mint_pubkey[CHIPPY_HEX_PUB_LEN + 1];
} chippy_chain;

/* util */
int chippy_hex_encode(const unsigned char *bin, size_t bin_len, char *hex, size_t hex_cap);
int chippy_hex_decode(const char *hex, unsigned char *bin, size_t bin_cap);
int chippy_str_eq(const char *a, const char *b);

/* crypto */
int chippy_keypair_generate(unsigned char *pk, unsigned char *sk);
int chippy_pubkey_to_address(const unsigned char *pk, char *addr_hex);
int chippy_address_to_pubkey(const char *addr_hex, unsigned char *pk);
int chippy_sign(const unsigned char *sk, const unsigned char *msg, size_t msg_len,
                unsigned char *sig);
int chippy_verify(const char *addr_hex, const unsigned char *msg, size_t msg_len,
                  const unsigned char *sig);
int chippy_sha256_hex(const unsigned char *data, size_t data_len, char *out_hex);

/* tx */
int chippy_tx_sign_payload(const chippy_tx *tx, unsigned char *buf, size_t buf_cap,
                           size_t *out_len);
int chippy_tx_verify(const chippy_tx *tx, const char *mint_pubkey);
int chippy_tx_parse_line(const char *line, chippy_tx *tx);
int chippy_tx_format_line(const chippy_tx *tx, char *line, size_t line_cap);

/* chain */
void chippy_chain_init(chippy_chain *chain);
void chippy_chain_free(chippy_chain *chain);
int chippy_block_compute_hash(const chippy_block *block, char *hash_out);
int chippy_chain_validate(const chippy_chain *chain);
int chippy_chain_balance(const chippy_chain *chain, const char *addr_hex, uint64_t *balance_out);
int chippy_chain_append_block(chippy_chain *chain, chippy_block *block);

/* storage */
int chippy_storage_init(const char *dir);
int chippy_storage_load_config(const char *dir, char *mint_pubkey_out);
int chippy_storage_load_chain(const char *dir, chippy_chain *chain);
int chippy_storage_append_block(const char *dir, const chippy_block *block);
int chippy_storage_wallet_new(const char *dir, const char *name, char *addr_out);
int chippy_storage_wallet_load_sec(const char *dir, const char *name, unsigned char *sk_out);
int chippy_storage_mint_load_sec(const char *dir, unsigned char *sk_out);
int chippy_storage_mint_pubkey(const char *dir, char *pub_out);

#endif /* CHIPPY_H */
