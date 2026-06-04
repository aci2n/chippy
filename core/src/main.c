#include "chippy.h"

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *data_dir = CHIPPY_DEFAULT_DIR;

static int lock_data_dir(void)
{
  if (chippy_dir_lock(data_dir) != 0) {
    fprintf(stderr, "data dir in use: %s\n", data_dir);
    return -1;
  }
  return 0;
}

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage:\n"
          "  %s init [--dir .chippy]\n"
          "  %s keygen\n"
          "  %s sign-transfer <secret_hex> <from> <to> <amount>\n"
          "  %s transfer <from> <to> <amount> <sig_hex> [--dir .chippy]\n"
          "  %s mint <to_addr> <amount> [--dir .chippy]\n"
          "  %s balance <addr> [--dir .chippy]\n"
          "  %s validate [--dir .chippy]\n",
          prog, prog, prog, prog, prog, prog, prog);
}

static void parse_global_dir(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--dir") == 0) {
      data_dir = argv[i + 1];
      return;
    }
  }
}

static int valid_address(const char *addr)
{
  return addr != NULL && strlen(addr) == CHIPPY_HEX_ADDR_LEN;
}

static int load_secret_hex(const char *hex, unsigned char *sk_out)
{
  int n;

  if (hex == NULL || sk_out == NULL) {
    return -1;
  }
  if (strlen(hex) != CHIPPY_HEX_SEC_LEN) {
    return -1;
  }
  n = chippy_hex_decode(hex, sk_out, crypto_sign_SECRETKEYBYTES);
  if (n != (int)crypto_sign_SECRETKEYBYTES) {
    return -1;
  }
  return 0;
}

static int cmd_init(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  if (lock_data_dir() != 0) {
    return 1;
  }
  if (chippy_storage_init(data_dir) != 0) {
    chippy_dir_unlock(data_dir);
    fprintf(stderr, "init failed\n");
    return 1;
  }
  chippy_dir_unlock(data_dir);
  printf("initialized data dir: %s\n", data_dir);
  return 0;
}

static int cmd_keygen(void)
{
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  char sec[CHIPPY_HEX_SEC_LEN + 1];

  if (chippy_keypair_generate(pk, sk) != 0) {
    fprintf(stderr, "keygen failed\n");
    return 1;
  }
  if (chippy_pubkey_to_address(pk, addr) != 0) {
    fprintf(stderr, "keygen failed\n");
    return 1;
  }
  if (chippy_hex_encode(sk, sizeof(sk), sec, sizeof(sec)) != 0) {
    fprintf(stderr, "keygen failed\n");
    return 1;
  }
  printf("address=%s\nsecret=%s\n", addr, sec);
  return 0;
}

static int cmd_sign_transfer(int argc, char **argv)
{
  chippy_tx tx;
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  char derived[CHIPPY_HEX_ADDR_LEN + 1];
  unsigned long long amount;

  if (argc < 6) {
    return -1;
  }
  if (!valid_address(argv[3]) || !valid_address(argv[4])) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  amount = strtoull(argv[5], NULL, 10);
  if (load_secret_hex(argv[2], sk) != 0) {
    fprintf(stderr, "invalid secret hex\n");
    return 1;
  }
  crypto_sign_ed25519_sk_to_pk(pk, sk);
  if (chippy_pubkey_to_address(pk, derived) != 0) {
    fprintf(stderr, "derive address failed\n");
    return 1;
  }
  if (!chippy_str_eq(derived, argv[3])) {
    fprintf(stderr, "secret does not match from address\n");
    return 1;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, argv[3], CHIPPY_HEX_ADDR_LEN);
  tx.from[CHIPPY_HEX_ADDR_LEN] = '\0';
  strncpy(tx.to, argv[4], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  if (chippy_tx_sign(&tx, sk) != 0) {
    fprintf(stderr, "sign failed\n");
    return 1;
  }
  printf("%s\n", tx.sig);
  return 0;
}

static int append_tx_block(chippy_chain *chain, const chippy_tx *tx)
{
  chippy_block block;
  const chippy_block *tip;
  chippy_tx *heap_tx;
  time_t now;

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
  if (chippy_storage_append_block(data_dir, &block) != 0) {
    free(heap_tx);
    return -1;
  }
  if (chippy_chain_append_block(chain, &block) != 0) {
    return -1;
  }
  return 0;
}

static int cmd_transfer(int argc, char **argv)
{
  chippy_chain chain;
  chippy_tx tx;
  unsigned long long amount;

  if (argc < 6) {
    return -1;
  }
  if (!valid_address(argv[2]) || !valid_address(argv[3])) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  if (strlen(argv[5]) != CHIPPY_HEX_SIG_LEN) {
    fprintf(stderr, "invalid signature\n");
    return 1;
  }
  amount = strtoull(argv[4], NULL, 10);
  if (lock_data_dir() != 0) {
    return 1;
  }
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    fprintf(stderr, "load chain failed\n");
    goto transfer_fail;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, argv[2], CHIPPY_HEX_ADDR_LEN);
  tx.from[CHIPPY_HEX_ADDR_LEN] = '\0';
  strncpy(tx.to, argv[3], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  strncpy(tx.sig, argv[5], CHIPPY_HEX_SIG_LEN);
  tx.sig[CHIPPY_HEX_SIG_LEN] = '\0';
  if (chippy_tx_verify(&tx, chain.mint_pubkey) != 0) {
    fprintf(stderr, "invalid transfer signature\n");
    goto transfer_fail;
  }
  if (append_tx_block(&chain, &tx) != 0) {
    fprintf(stderr, "append block failed (insufficient balance?)\n");
    goto transfer_fail;
  }
  chippy_chain_free(&chain);
  chippy_dir_unlock(data_dir);
  printf("transferred %llu from %s to %s\n", (unsigned long long)amount, argv[2], argv[3]);
  return 0;
transfer_fail:
  chippy_chain_free(&chain);
  chippy_dir_unlock(data_dir);
  return 1;
}

static int cmd_mint(int argc, char **argv)
{
  chippy_chain chain;
  chippy_tx tx;
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  unsigned long long amount;

  if (argc < 4) {
    return -1;
  }
  if (!valid_address(argv[2])) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  amount = strtoull(argv[3], NULL, 10);
  if (lock_data_dir() != 0) {
    return 1;
  }
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    fprintf(stderr, "load chain failed\n");
    goto mint_fail;
  }
  if (chippy_storage_mint_load_sec(data_dir, sk) != 0) {
    fprintf(stderr, "load mint key failed\n");
    goto mint_fail;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, argv[2], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  if (chippy_tx_sign(&tx, sk) != 0) {
    fprintf(stderr, "sign failed\n");
    goto mint_fail;
  }
  if (append_tx_block(&chain, &tx) != 0) {
    fprintf(stderr, "append block failed\n");
    goto mint_fail;
  }
  chippy_chain_free(&chain);
  chippy_dir_unlock(data_dir);
  printf("minted %llu to %s\n", (unsigned long long)amount, argv[2]);
  return 0;
mint_fail:
  chippy_chain_free(&chain);
  chippy_dir_unlock(data_dir);
  return 1;
}

static int cmd_balance(int argc, char **argv)
{
  chippy_chain chain;
  uint64_t balance;

  if (argc < 3) {
    return -1;
  }
  if (!valid_address(argv[2])) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  if (lock_data_dir() != 0) {
    return 1;
  }
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    chippy_dir_unlock(data_dir);
    fprintf(stderr, "load chain failed\n");
    return 1;
  }
  if (chippy_chain_balance(&chain, argv[2], &balance) != 0) {
    chippy_chain_free(&chain);
    chippy_dir_unlock(data_dir);
    fprintf(stderr, "balance failed\n");
    return 1;
  }
  chippy_chain_free(&chain);
  chippy_dir_unlock(data_dir);
  printf("%llu\n", (unsigned long long)balance);
  return 0;
}

static int cmd_validate(int argc, char **argv)
{
  chippy_chain chain;

  (void)argc;
  (void)argv;
  if (lock_data_dir() != 0) {
    return 1;
  }
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    chippy_dir_unlock(data_dir);
    fprintf(stderr, "load chain failed\n");
    return 1;
  }
  if (chippy_chain_validate(&chain) != 0) {
    chippy_chain_free(&chain);
    chippy_dir_unlock(data_dir);
    fprintf(stderr, "chain invalid\n");
    return 1;
  }
  chippy_chain_free(&chain);
  chippy_dir_unlock(data_dir);
  printf("ok\n");
  return 0;
}

int main(int argc, char **argv)
{
  int rc;

  if (sodium_init() < 0) {
    fprintf(stderr, "sodium_init failed\n");
    return 1;
  }
  parse_global_dir(argc, argv);
  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  rc = 1;
  if (strcmp(argv[1], "init") == 0) {
    rc = cmd_init(argc, argv);
  } else if (strcmp(argv[1], "keygen") == 0) {
    rc = cmd_keygen();
  } else if (strcmp(argv[1], "sign-transfer") == 0) {
    rc = cmd_sign_transfer(argc, argv);
  } else if (strcmp(argv[1], "transfer") == 0) {
    rc = cmd_transfer(argc, argv);
  } else if (strcmp(argv[1], "mint") == 0) {
    rc = cmd_mint(argc, argv);
  } else if (strcmp(argv[1], "balance") == 0) {
    rc = cmd_balance(argc, argv);
  } else if (strcmp(argv[1], "validate") == 0) {
    rc = cmd_validate(argc, argv);
  } else {
    usage(argv[0]);
    rc = 1;
  }
  if (rc < 0) {
    usage(argv[0]);
    rc = 1;
  }
  return rc;
}
