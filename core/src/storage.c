#include "chippy.h"

#include <sodium.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define CHIPPY_ZERO_HASH "0000000000000000000000000000000000000000000000000000000000000000"

static int mkdir_p(const char *path)
{
  char tmp[CHIPPY_MAX_PATH];
  size_t len;
  size_t i;

  if (path == NULL) {
    return -1;
  }
  strncpy(tmp, path, sizeof(tmp) - 1);
  tmp[sizeof(tmp) - 1] = '\0';
  len = strlen(tmp);
  if (len == 0) {
    return -1;
  }
  for (i = 1; i < len; i++) {
    if (tmp[i] == '/') {
      tmp[i] = '\0';
      if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
        return -1;
      }
      tmp[i] = '/';
    }
  }
  if (mkdir(tmp, 0700) != 0 && errno != EEXIST) {
    return -1;
  }
  return 0;
}

static int path_join(char *out, size_t out_cap, const char *dir, const char *name)
{
  int n;

  n = snprintf(out, out_cap, "%s/%s", dir, name);
  if (n < 0 || (size_t)n >= out_cap) {
    return -1;
  }
  return 0;
}

static int chain_grow_blocks(chippy_chain *chain)
{
  chippy_block *new_blocks;
  size_t new_cap;

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

static int push_block(chippy_chain *chain, chippy_block *block)
{
  if (chain_grow_blocks(chain) != 0) {
    return -1;
  }
  chain->blocks[chain->block_count++] = *block;
  return 0;
}

int chippy_storage_init(const char *dir, char *mint_address_out, char *mint_secret_out)
{
  char config_path[CHIPPY_MAX_PATH];
  char chain_path[CHIPPY_MAX_PATH];
  char mint_addr[CHIPPY_HEX_ADDR_LEN + 1];
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  chippy_block genesis;
  FILE *f;
  time_t now;

  if (dir == NULL) {
    return -1;
  }
  if (mkdir_p(dir) != 0) {
    return -1;
  }

  if (chippy_keypair_generate(pk, sk) != 0) {
    return -1;
  }
  if (chippy_pubkey_to_address(pk, mint_addr) != 0) {
    return -1;
  }
  if (mint_address_out != NULL) {
    strncpy(mint_address_out, mint_addr, CHIPPY_HEX_ADDR_LEN);
    mint_address_out[CHIPPY_HEX_ADDR_LEN] = '\0';
  }
  if (mint_secret_out != NULL) {
    if (chippy_hex_encode(sk, sizeof(sk), mint_secret_out, CHIPPY_HEX_SEC_LEN + 1) != 0) {
      return -1;
    }
  }

  if (path_join(config_path, sizeof(config_path), dir, "config") != 0) {
    return -1;
  }
  f = fopen(config_path, "w");
  if (f == NULL) {
    return -1;
  }
  fprintf(f, "mint_pubkey=%s\n", mint_addr);
  fclose(f);

  memset(&genesis, 0, sizeof(genesis));
  genesis.index = 0;
  now = time(NULL);
  genesis.timestamp = (uint64_t)now;
  strncpy(genesis.prev_hash, CHIPPY_ZERO_HASH, CHIPPY_HEX_HASH_LEN);
  genesis.prev_hash[CHIPPY_HEX_HASH_LEN] = '\0';
  genesis.txs = NULL;
  genesis.tx_count = 0;
  if (chippy_block_compute_hash(&genesis, genesis.hash) != 0) {
    return -1;
  }

  if (path_join(chain_path, sizeof(chain_path), dir, "chain") != 0) {
    return -1;
  }
  f = fopen(chain_path, "w");
  if (f == NULL) {
    return -1;
  }
  fprintf(f, "block %llu\n", (unsigned long long)genesis.index);
  fprintf(f, "time %llu\n", (unsigned long long)genesis.timestamp);
  fprintf(f, "prev %s\n", genesis.prev_hash);
  fprintf(f, "hash %s\n", genesis.hash);
  fprintf(f, "\n");
  fclose(f);
  return 0;
}

int chippy_storage_load_config(const char *dir, char *mint_pubkey_out)
{
  char config_path[CHIPPY_MAX_PATH];
  FILE *f;
  char line[256];
  char *eq;

  if (dir == NULL || mint_pubkey_out == NULL) {
    return -1;
  }
  if (path_join(config_path, sizeof(config_path), dir, "config") != 0) {
    return -1;
  }
  f = fopen(config_path, "r");
  if (f == NULL) {
    return -1;
  }
  while (fgets(line, sizeof(line), f) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    if (strncmp(line, "mint_pubkey=", 12) != 0) {
      continue;
    }
    eq = line + 12;
    if (strlen(eq) != CHIPPY_HEX_PUB_LEN) {
      fclose(f);
      return -1;
    }
    strncpy(mint_pubkey_out, eq, CHIPPY_HEX_PUB_LEN);
    mint_pubkey_out[CHIPPY_HEX_PUB_LEN] = '\0';
    fclose(f);
    return 0;
  }
  fclose(f);
  return -1;
}

static void free_parsed_block(chippy_block *block)
{
  if (block == NULL) {
    return;
  }
  free(block->txs);
  block->txs = NULL;
  block->tx_count = 0;
}

static int grow_txs(chippy_block *block)
{
  chippy_tx *new_txs;

  new_txs =
      realloc(block->txs, (block->tx_count + 1) * sizeof(chippy_tx));
  if (new_txs == NULL) {
    return -1;
  }
  block->txs = new_txs;
  return 0;
}

int chippy_storage_load_chain(const char *dir, chippy_chain *chain)
{
  char chain_path[CHIPPY_MAX_PATH];
  FILE *f;
  char line[1024];
  chippy_block cur;
  int in_block;
  unsigned long long val;

  if (dir == NULL || chain == NULL) {
    return -1;
  }
  chippy_chain_init(chain);
  if (chippy_storage_load_config(dir, chain->mint_pubkey) != 0) {
    return -1;
  }
  if (path_join(chain_path, sizeof(chain_path), dir, "chain") != 0) {
    return -1;
  }
  f = fopen(chain_path, "r");
  if (f == NULL) {
    return -1;
  }

  memset(&cur, 0, sizeof(cur));
  in_block = 0;
  while (fgets(line, sizeof(line), f) != NULL) {
    line[strcspn(line, "\r\n")] = '\0';
    if (line[0] == '\0') {
      if (in_block) {
        if (push_block(chain, &cur) != 0) {
          free_parsed_block(&cur);
          fclose(f);
          chippy_chain_free(chain);
          return -1;
        }
        cur.txs = NULL;
        cur.tx_count = 0;
        in_block = 0;
      }
      continue;
    }
    if (strncmp(line, "block ", 6) == 0) {
      if (in_block) {
        if (push_block(chain, &cur) != 0) {
          free_parsed_block(&cur);
          fclose(f);
          chippy_chain_free(chain);
          return -1;
        }
        cur.txs = NULL;
        cur.tx_count = 0;
      }
      if (sscanf(line + 6, "%llu", &val) != 1) {
        fclose(f);
        chippy_chain_free(chain);
        return -1;
      }
      cur.index = (uint64_t)val;
      in_block = 1;
      continue;
    }
    if (!in_block) {
      continue;
    }
    if (strncmp(line, "time ", 5) == 0) {
      if (sscanf(line + 5, "%llu", &val) != 1) {
        fclose(f);
        chippy_chain_free(chain);
        return -1;
      }
      cur.timestamp = (uint64_t)val;
      continue;
    }
    if (strncmp(line, "prev ", 5) == 0) {
      if (sscanf(line + 5, "%64s", cur.prev_hash) != 1) {
        fclose(f);
        chippy_chain_free(chain);
        return -1;
      }
      continue;
    }
    if (strncmp(line, "hash ", 5) == 0) {
      if (sscanf(line + 5, "%64s", cur.hash) != 1) {
        fclose(f);
        chippy_chain_free(chain);
        return -1;
      }
      continue;
    }
    if (strncmp(line, "tx ", 3) == 0) {
      chippy_tx tx;
      if (chippy_tx_parse_line(line + 3, &tx) != 0) {
        fclose(f);
        chippy_chain_free(chain);
        return -1;
      }
      if (grow_txs(&cur) != 0) {
        fclose(f);
        chippy_chain_free(chain);
        return -1;
      }
      cur.txs[cur.tx_count++] = tx;
      continue;
    }
  }
  if (in_block) {
    if (push_block(chain, &cur) != 0) {
      free_parsed_block(&cur);
      fclose(f);
      chippy_chain_free(chain);
      return -1;
    }
  }
  fclose(f);
  return 0;
}

int chippy_storage_append_block(const char *dir, const chippy_block *block)
{
  char chain_path[CHIPPY_MAX_PATH];
  FILE *f;
  size_t i;
  char line[1024];

  if (dir == NULL || block == NULL) {
    return -1;
  }
  if (path_join(chain_path, sizeof(chain_path), dir, "chain") != 0) {
    return -1;
  }
  f = fopen(chain_path, "a");
  if (f == NULL) {
    return -1;
  }
  fprintf(f, "block %llu\n", (unsigned long long)block->index);
  fprintf(f, "time %llu\n", (unsigned long long)block->timestamp);
  fprintf(f, "prev %s\n", block->prev_hash);
  for (i = 0; i < block->tx_count; i++) {
    if (chippy_tx_format_line(&block->txs[i], line, sizeof(line)) != 0) {
      fclose(f);
      return -1;
    }
    fprintf(f, "tx %s\n", line);
  }
  fprintf(f, "hash %s\n", block->hash);
  fprintf(f, "\n");
  fclose(f);
  return 0;
}

int chippy_storage_mint_pubkey(const char *dir, char *pub_out)
{
  return chippy_storage_load_config(dir, pub_out);
}
