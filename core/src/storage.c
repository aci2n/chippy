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

static int write_keypair_files(const char *pub_path, const char *sec_path,
                               const unsigned char *pk, const unsigned char *sk)
{
  char hex[CHIPPY_HEX_SEC_LEN + 1];
  FILE *f;

  if (chippy_hex_encode(pk, crypto_sign_PUBLICKEYBYTES, hex, sizeof(hex)) != 0) {
    return -1;
  }
  f = fopen(pub_path, "w");
  if (f == NULL) {
    return -1;
  }
  fprintf(f, "%s\n", hex);
  fclose(f);

  if (chippy_hex_encode(sk, crypto_sign_SECRETKEYBYTES, hex, sizeof(hex)) != 0) {
    return -1;
  }
  f = fopen(sec_path, "w");
  if (f == NULL) {
    return -1;
  }
  fprintf(f, "%s\n", hex);
  fclose(f);
  return 0;
}

static int read_hex_file(const char *path, unsigned char *buf, size_t buf_len, size_t expect_hex)
{
  char hex[CHIPPY_HEX_SEC_LEN + 1];
  FILE *f;
  int n;

  f = fopen(path, "r");
  if (f == NULL) {
    return -1;
  }
  if (fgets(hex, (int)sizeof(hex), f) == NULL) {
    fclose(f);
    return -1;
  }
  fclose(f);
  hex[strcspn(hex, "\r\n")] = '\0';
  if (strlen(hex) != expect_hex) {
    return -1;
  }
  n = chippy_hex_decode(hex, buf, buf_len);
  if (n < 0) {
    return -1;
  }
  return n;
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

int chippy_storage_init(const char *dir)
{
  char wallets[CHIPPY_MAX_PATH];
  char config_path[CHIPPY_MAX_PATH];
  char pub_path[CHIPPY_MAX_PATH];
  char sec_path[CHIPPY_MAX_PATH];
  char chain_path[CHIPPY_MAX_PATH];
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
  if (path_join(wallets, sizeof(wallets), dir, "wallets") != 0) {
    return -1;
  }
  if (mkdir_p(wallets) != 0) {
    return -1;
  }

  if (chippy_keypair_generate(pk, sk) != 0) {
    return -1;
  }
  if (path_join(pub_path, sizeof(pub_path), dir, "mint.pub") != 0) {
    return -1;
  }
  if (path_join(sec_path, sizeof(sec_path), dir, "mint.sec") != 0) {
    return -1;
  }
  if (write_keypair_files(pub_path, sec_path, pk, sk) != 0) {
    return -1;
  }

  if (path_join(config_path, sizeof(config_path), dir, "config") != 0) {
    return -1;
  }
  f = fopen(config_path, "w");
  if (f == NULL) {
    return -1;
  }
  if (chippy_pubkey_to_address(pk, genesis.hash) != 0) {
    fclose(f);
    return -1;
  }
  fprintf(f, "mint_pubkey=%s\n", genesis.hash);
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

int chippy_storage_wallet_new(const char *dir, const char *name, char *addr_out)
{
  char wallets[CHIPPY_MAX_PATH];
  char pub_path[CHIPPY_MAX_PATH];
  char sec_path[CHIPPY_MAX_PATH];
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];

  if (dir == NULL || name == NULL || addr_out == NULL) {
    return -1;
  }
  if (path_join(wallets, sizeof(wallets), dir, "wallets") != 0) {
    return -1;
  }
  if (mkdir_p(wallets) != 0) {
    return -1;
  }
  if (snprintf(pub_path, sizeof(pub_path), "%s/%s.pub", wallets, name) >= (int)sizeof(pub_path)) {
    return -1;
  }
  if (snprintf(sec_path, sizeof(sec_path), "%s/%s.sec", wallets, name) >= (int)sizeof(sec_path)) {
    return -1;
  }
  if (chippy_keypair_generate(pk, sk) != 0) {
    return -1;
  }
  if (write_keypair_files(pub_path, sec_path, pk, sk) != 0) {
    return -1;
  }
  return chippy_pubkey_to_address(pk, addr_out);
}

int chippy_storage_wallet_load_sec(const char *dir, const char *name, unsigned char *sk_out)
{
  char path[CHIPPY_MAX_PATH];

  if (dir == NULL || name == NULL || sk_out == NULL) {
    return -1;
  }
  if (snprintf(path, sizeof(path), "%s/wallets/%s.sec", dir, name) >= (int)sizeof(path)) {
    return -1;
  }
  if (read_hex_file(path, sk_out, crypto_sign_SECRETKEYBYTES, CHIPPY_HEX_SEC_LEN) !=
      (int)crypto_sign_SECRETKEYBYTES) {
    return -1;
  }
  return 0;
}

int chippy_storage_mint_load_sec(const char *dir, unsigned char *sk_out)
{
  char path[CHIPPY_MAX_PATH];

  if (dir == NULL || sk_out == NULL) {
    return -1;
  }
  if (path_join(path, sizeof(path), dir, "mint.sec") != 0) {
    return -1;
  }
  if (read_hex_file(path, sk_out, crypto_sign_SECRETKEYBYTES, CHIPPY_HEX_SEC_LEN) !=
      (int)crypto_sign_SECRETKEYBYTES) {
    return -1;
  }
  return 0;
}

int chippy_storage_mint_pubkey(const char *dir, char *pub_out)
{
  return chippy_storage_load_config(dir, pub_out);
}
