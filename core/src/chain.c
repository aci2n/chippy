#include "chippy.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHIPPY_ZERO_HASH "0000000000000000000000000000000000000000000000000000000000000000"

typedef struct {
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  uint64_t balance;
} balance_entry;

typedef struct {
  balance_entry *entries;
  size_t count;
  size_t cap;
} balance_map;

static int balance_get(balance_map *map, const char *addr, uint64_t *out)
{
  size_t i;

  if (map == NULL || addr == NULL || out == NULL) {
    return -1;
  }
  for (i = 0; i < map->count; i++) {
    if (chippy_str_eq(map->entries[i].addr, addr)) {
      *out = map->entries[i].balance;
      return 0;
    }
  }
  *out = 0;
  return 0;
}

static int balance_set(balance_map *map, const char *addr, uint64_t value)
{
  size_t i;

  if (map == NULL || addr == NULL) {
    return -1;
  }
  for (i = 0; i < map->count; i++) {
    if (chippy_str_eq(map->entries[i].addr, addr)) {
      map->entries[i].balance = value;
      return 0;
    }
  }
  if (map->count >= map->cap) {
    size_t new_cap = map->cap == 0 ? 8 : map->cap * 2;
    balance_entry *new_entries =
        realloc(map->entries, new_cap * sizeof(balance_entry));
    if (new_entries == NULL) {
      return -1;
    }
    map->entries = new_entries;
    map->cap = new_cap;
  }
  strncpy(map->entries[map->count].addr, addr, CHIPPY_HEX_ADDR_LEN);
  map->entries[map->count].addr[CHIPPY_HEX_ADDR_LEN] = '\0';
  map->entries[map->count].balance = value;
  map->count++;
  return 0;
}

static void balance_map_free(balance_map *map)
{
  if (map == NULL) {
    return;
  }
  free(map->entries);
  map->entries = NULL;
  map->count = 0;
  map->cap = 0;
}

static int apply_tx(balance_map *map, const chippy_tx *tx, const chippy_mint_keys *mint_keys)
{
  uint64_t from_bal;
  uint64_t to_bal;

  if (map == NULL || tx == NULL || mint_keys == NULL) {
    return -1;
  }
  if (chippy_tx_verify(tx, mint_keys) != 0) {
    LOG_DEBUG("apply_tx verify failed type=%d", (int)tx->type);
    return -1;
  }
  if (tx->type == CHIPPY_TX_MINT) {
    LOG_DEBUG("apply_tx mint to=%s amount=%llu", tx->to, (unsigned long long)tx->amount);
    if (balance_get(map, tx->to, &to_bal) != 0) {
      return -1;
    }
    if (balance_set(map, tx->to, to_bal + tx->amount) != 0) {
      return -1;
    }
    return 0;
  }
  if (balance_get(map, tx->from, &from_bal) != 0) {
    return -1;
  }
  if (from_bal < tx->amount) {
    LOG_DEBUG("apply_tx insufficient funds from=%s have=%llu need=%llu", tx->from,
              (unsigned long long)from_bal, (unsigned long long)tx->amount);
    return -1;
  }
  LOG_DEBUG("apply_tx transfer from=%s to=%s amount=%llu", tx->from, tx->to,
            (unsigned long long)tx->amount);
  if (balance_get(map, tx->to, &to_bal) != 0) {
    return -1;
  }
  if (balance_set(map, tx->from, from_bal - tx->amount) != 0) {
    return -1;
  }
  if (balance_set(map, tx->to, to_bal + tx->amount) != 0) {
    return -1;
  }
  return 0;
}

static int apply_block(balance_map *map, const chippy_block *block, const chippy_mint_keys *mint_keys)
{
  size_t i;

  if (map == NULL || block == NULL || mint_keys == NULL) {
    return -1;
  }
  for (i = 0; i < block->tx_count; i++) {
    if (apply_tx(map, &block->txs[i], mint_keys) != 0) {
      return -1;
    }
  }
  return 0;
}

static int block_hash_input(const chippy_block *block, unsigned char *buf, size_t buf_cap,
                            size_t *out_len)
{
  size_t off;
  int n;
  unsigned char payload[512];
  size_t payload_len;
  size_t i;

  if (block == NULL || buf == NULL || out_len == NULL) {
    return -1;
  }
  off = 0;
  n = snprintf((char *)buf + off, buf_cap - off, "index %llu\ntime %llu\nprev %s\n",
               (unsigned long long)block->index, (unsigned long long)block->timestamp,
               block->prev_hash);
  if (n < 0 || (size_t)n >= buf_cap - off) {
    return -1;
  }
  off += (size_t)n;
  for (i = 0; i < block->tx_count; i++) {
    if (chippy_tx_sign_payload(&block->txs[i], payload, sizeof(payload), &payload_len) != 0) {
      return -1;
    }
    n = snprintf((char *)buf + off, buf_cap - off, "tx %.*s %s\n", (int)payload_len,
                 (char *)payload, block->txs[i].sig);
    if (n < 0 || (size_t)n >= buf_cap - off) {
      return -1;
    }
    off += (size_t)n;
  }
  *out_len = off;
  return 0;
}

static int chain_grow(chippy_chain *chain)
{
  chippy_block *new_blocks;
  size_t new_cap;

  if (chain == NULL) {
    return -1;
  }
  if (chain->block_count < chain->block_cap) {
    return 0;
  }
  new_cap = chain->block_cap == 0 ? 4 : chain->block_cap * 2;
  new_blocks = realloc(chain->blocks, new_cap * sizeof(chippy_block));
  if (new_blocks == NULL) {
    return -1;
  }
  chain->blocks = new_blocks;
  chain->block_cap = new_cap;
  return 0;
}

void chippy_chain_init(chippy_chain *chain)
{
  if (chain == NULL) {
    return;
  }
  chain->blocks = NULL;
  chain->block_count = 0;
  chain->block_cap = 0;
  chippy_mint_keys_init(&chain->mint_keys);
}

void chippy_chain_free(chippy_chain *chain)
{
  size_t i;

  if (chain == NULL) {
    return;
  }
  for (i = 0; i < chain->block_count; i++) {
    free(chain->blocks[i].txs);
    chain->blocks[i].txs = NULL;
    chain->blocks[i].tx_count = 0;
  }
  free(chain->blocks);
  chain->blocks = NULL;
  chain->block_count = 0;
  chain->block_cap = 0;
  chippy_mint_keys_init(&chain->mint_keys);
}

int chippy_block_compute_hash(const chippy_block *block, char *hash_out)
{
  unsigned char buf[65536];
  size_t len;

  if (block == NULL || hash_out == NULL) {
    return -1;
  }
  if (block_hash_input(block, buf, sizeof(buf), &len) != 0) {
    return -1;
  }
  return chippy_sha256_hex(buf, len, hash_out);
}

static int validate_block_links(const chippy_chain *chain, size_t idx)
{
  const chippy_block *block;
  const chippy_block *prev;

  block = &chain->blocks[idx];
  if (idx == 0) {
    if (!chippy_str_eq(block->prev_hash, CHIPPY_ZERO_HASH)) {
      return -1;
    }
    if (block->index != 0) {
      return -1;
    }
    return 0;
  }
  prev = &chain->blocks[idx - 1];
  if (block->index != prev->index + 1) {
    return -1;
  }
  if (!chippy_str_eq(block->prev_hash, prev->hash)) {
    return -1;
  }
  return 0;
}

int chippy_chain_validate(const chippy_chain *chain)
{
  balance_map map;
  char computed[CHIPPY_HEX_HASH_LEN + 1];
  size_t i;

  if (chain == NULL) {
    return -1;
  }
  LOG_DEBUG("chain_validate blocks=%zu mint_keys=%zu", chain->block_count, chain->mint_keys.count);
  if (chain->mint_keys.count == 0) {
    return -1;
  }
  if (chain->block_count == 0) {
    return -1;
  }
  memset(&map, 0, sizeof(map));
  for (i = 0; i < chain->block_count; i++) {
    if (validate_block_links(chain, i) != 0) {
      LOG_DEBUG("chain_validate bad link at block %zu", i);
      balance_map_free(&map);
      return -1;
    }
    if (chippy_block_compute_hash(&chain->blocks[i], computed) != 0) {
      balance_map_free(&map);
      return -1;
    }
    if (!chippy_str_eq(computed, chain->blocks[i].hash)) {
      LOG_DEBUG("chain_validate hash mismatch at block %zu", i);
      balance_map_free(&map);
      return -1;
    }
    if (apply_block(&map, &chain->blocks[i], &chain->mint_keys) != 0) {
      LOG_DEBUG("chain_validate apply failed at block %zu", i);
      balance_map_free(&map);
      return -1;
    }
  }
  balance_map_free(&map);
  LOG_DEBUG("chain_validate ok");
  return 0;
}

int chippy_chain_balance(const chippy_chain *chain, const char *addr_hex, uint64_t *balance_out)
{
  balance_map map;
  size_t i;

  if (chain == NULL || addr_hex == NULL || balance_out == NULL) {
    return -1;
  }
  memset(&map, 0, sizeof(map));
  for (i = 0; i < chain->block_count; i++) {
    if (apply_block(&map, &chain->blocks[i], &chain->mint_keys) != 0) {
      balance_map_free(&map);
      return -1;
    }
  }
  if (balance_get(&map, addr_hex, balance_out) != 0) {
    balance_map_free(&map);
    return -1;
  }
  balance_map_free(&map);
  return 0;
}

int chippy_chain_append_block(chippy_chain *chain, chippy_block *block)
{
  chippy_chain replay;
  size_t i;
  chippy_block copy;

  if (chain == NULL || block == NULL) {
    return -1;
  }
  LOG_DEBUG("chain_append_block new_index=%llu txs=%zu", (unsigned long long)block->index,
            block->tx_count);
  chippy_chain_init(&replay);
  chippy_mint_keys_copy(&replay.mint_keys, &chain->mint_keys);
  for (i = 0; i < chain->block_count; i++) {
    if (chain_grow(&replay) != 0) {
      chippy_chain_free(&replay);
      return -1;
    }
    copy = chain->blocks[i];
    copy.txs = NULL;
    copy.tx_count = 0;
    if (chain->blocks[i].tx_count > 0) {
      copy.txs = calloc(chain->blocks[i].tx_count, sizeof(chippy_tx));
      if (copy.txs == NULL) {
        chippy_chain_free(&replay);
        return -1;
      }
      memcpy(copy.txs, chain->blocks[i].txs, chain->blocks[i].tx_count * sizeof(chippy_tx));
      copy.tx_count = chain->blocks[i].tx_count;
    }
    replay.blocks[replay.block_count++] = copy;
  }
  if (chain_grow(&replay) != 0) {
    chippy_chain_free(&replay);
    return -1;
  }
  replay.blocks[replay.block_count++] = *block;
  if (chippy_chain_validate(&replay) != 0) {
    LOG_DEBUG("chain_append_block replay validation failed");
    if (replay.block_count > 0) {
      replay.blocks[replay.block_count - 1].txs = NULL;
      replay.blocks[replay.block_count - 1].tx_count = 0;
    }
    chippy_chain_free(&replay);
    free(block->txs);
    block->txs = NULL;
    block->tx_count = 0;
    return -1;
  }
  if (replay.block_count > 0) {
    replay.blocks[replay.block_count - 1].txs = NULL;
    replay.blocks[replay.block_count - 1].tx_count = 0;
  }
  chippy_chain_free(&replay);

  if (chain_grow(chain) != 0) {
    return -1;
  }
  chain->blocks[chain->block_count++] = *block;
  block->txs = NULL;
  block->tx_count = 0;
  return 0;
}
