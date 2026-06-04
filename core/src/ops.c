#include "chippy_ops.h"

#include <sodium.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static int address_valid(const char *addr)
{
  return addr != NULL && strlen(addr) == CHIPPY_HEX_ADDR_LEN;
}

static int append_tx_block(const char *dir, chippy_chain *chain, const chippy_tx *tx)
{
  chippy_block block;
  const chippy_block *tip;
  chippy_tx *heap_tx;
  time_t now;

  if (dir == NULL || chain == NULL || tx == NULL) {
    return -1;
  }
  heap_tx = malloc(sizeof(chippy_tx));
  if (heap_tx == NULL) {
    return -1;
  }
  *heap_tx = *tx;
  memset(&block, 0, sizeof(block));
  block.txs = heap_tx;
  block.tx_count = 1;
  if (chain->block_count == 0) {
    free(heap_tx);
    return -1;
  }
  tip = &chain->blocks[chain->block_count - 1];
  block.index = tip->index + 1;
  now = time(NULL);
  block.timestamp = (uint64_t)now;
  strncpy(block.prev_hash, tip->hash, CHIPPY_HEX_HASH_LEN);
  block.prev_hash[CHIPPY_HEX_HASH_LEN] = '\0';
  if (chippy_block_compute_hash(&block, block.hash) != 0) {
    free(heap_tx);
    return -1;
  }
  if (chippy_storage_append_block(dir, &block) != 0) {
    free(heap_tx);
    return -1;
  }
  if (chippy_chain_append_block(chain, &block) != 0) {
    return -1;
  }
  return 0;
}

int chippy_op_init(const char *dir)
{
  if (dir == NULL) {
    return -1;
  }
  if (chippy_dir_lock(dir) != 0) {
    return -1;
  }
  if (chippy_storage_init(dir) != 0) {
    chippy_dir_unlock(dir);
    return -1;
  }
  return chippy_dir_unlock(dir);
}

int chippy_op_mint(const char *dir, const char *to_addr, uint64_t amount)
{
  chippy_chain chain;
  chippy_tx tx;
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  int rc;

  if (dir == NULL || !address_valid(to_addr)) {
    return -1;
  }
  if (chippy_dir_lock(dir) != 0) {
    return -1;
  }
  rc = -1;
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(dir, &chain) != 0) {
    goto done;
  }
  if (chippy_storage_mint_load_sec(dir, sk) != 0) {
    goto done;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, to_addr, CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = amount;
  if (chippy_tx_sign(&tx, sk) != 0) {
    goto done;
  }
  if (append_tx_block(dir, &chain, &tx) != 0) {
    goto done;
  }
  rc = 0;
done:
  chippy_chain_free(&chain);
  if (chippy_dir_unlock(dir) != 0) {
    return -1;
  }
  return rc;
}

int chippy_op_transfer(const char *dir, const chippy_tx *tx)
{
  chippy_chain chain;
  chippy_tx copy;
  int rc;

  if (dir == NULL || tx == NULL) {
    return -1;
  }
  if (tx->type != CHIPPY_TX_TRANSFER) {
    return -1;
  }
  if (!address_valid(tx->from) || !address_valid(tx->to)) {
    return -1;
  }
  if (strlen(tx->sig) != CHIPPY_HEX_SIG_LEN) {
    return -1;
  }
  if (chippy_dir_lock(dir) != 0) {
    return -1;
  }
  rc = -1;
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(dir, &chain) != 0) {
    goto done;
  }
  copy = *tx;
  if (chippy_tx_verify(&copy, chain.mint_pubkey) != 0) {
    goto done;
  }
  if (append_tx_block(dir, &chain, &copy) != 0) {
    goto done;
  }
  rc = 0;
done:
  chippy_chain_free(&chain);
  if (chippy_dir_unlock(dir) != 0) {
    return -1;
  }
  return rc;
}

int chippy_op_balance(const char *dir, const char *addr_hex, uint64_t *balance_out)
{
  chippy_chain chain;
  int rc;

  if (dir == NULL || !address_valid(addr_hex) || balance_out == NULL) {
    return -1;
  }
  if (chippy_dir_lock(dir) != 0) {
    return -1;
  }
  rc = -1;
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(dir, &chain) != 0) {
    goto done;
  }
  if (chippy_chain_balance(&chain, addr_hex, balance_out) != 0) {
    goto done;
  }
  rc = 0;
done:
  chippy_chain_free(&chain);
  if (chippy_dir_unlock(dir) != 0) {
    return -1;
  }
  return rc;
}

int chippy_op_validate(const char *dir)
{
  chippy_chain chain;
  int rc;

  if (dir == NULL) {
    return -1;
  }
  if (chippy_dir_lock(dir) != 0) {
    return -1;
  }
  rc = -1;
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(dir, &chain) != 0) {
    goto done;
  }
  if (chippy_chain_validate(&chain) != 0) {
    goto done;
  }
  rc = 0;
done:
  chippy_chain_free(&chain);
  if (chippy_dir_unlock(dir) != 0) {
    return -1;
  }
  return rc;
}

int chippy_op_keygen(char *addr_out, char *secret_hex_out)
{
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];

  if (addr_out == NULL || secret_hex_out == NULL) {
    return -1;
  }
  if (chippy_keypair_generate(pk, sk) != 0) {
    return -1;
  }
  if (chippy_pubkey_to_address(pk, addr_out) != 0) {
    return -1;
  }
  return chippy_hex_encode(sk, sizeof(sk), secret_hex_out, CHIPPY_HEX_SEC_LEN + 1);
}

int chippy_op_sign_transfer(const char *secret_hex, const char *from_addr, const char *to_addr,
                            uint64_t amount, char *sig_out)
{
  chippy_tx tx;
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  char derived[CHIPPY_HEX_ADDR_LEN + 1];
  int n;

  if (secret_hex == NULL || from_addr == NULL || to_addr == NULL || sig_out == NULL) {
    return -1;
  }
  if (!address_valid(from_addr) || !address_valid(to_addr)) {
    return -1;
  }
  if (strlen(secret_hex) != CHIPPY_HEX_SEC_LEN) {
    return -1;
  }
  n = chippy_hex_decode(secret_hex, sk, sizeof(sk));
  if (n != (int)crypto_sign_SECRETKEYBYTES) {
    return -1;
  }
  crypto_sign_ed25519_sk_to_pk(pk, sk);
  if (chippy_pubkey_to_address(pk, derived) != 0) {
    return -1;
  }
  if (!chippy_str_eq(derived, from_addr)) {
    return -1;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, from_addr, CHIPPY_HEX_ADDR_LEN);
  tx.from[CHIPPY_HEX_ADDR_LEN] = '\0';
  strncpy(tx.to, to_addr, CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = amount;
  if (chippy_tx_sign(&tx, sk) != 0) {
    return -1;
  }
  strncpy(sig_out, tx.sig, CHIPPY_HEX_SIG_LEN);
  sig_out[CHIPPY_HEX_SIG_LEN] = '\0';
  return 0;
}
