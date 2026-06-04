#include "chippy.h"

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *data_dir = CHIPPY_DEFAULT_DIR;

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage:\n"
          "  %s init [--dir .chippy]\n"
          "  %s wallet new <name>\n"
          "  %s mint <to_addr> <amount>\n"
          "  %s send <wallet> <to_addr> <amount>\n"
          "  %s balance <addr>\n"
          "  %s validate [--dir .chippy]\n",
          prog, prog, prog, prog, prog, prog);
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

static int cmd_init(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  if (chippy_storage_init(data_dir) != 0) {
    fprintf(stderr, "init failed\n");
    return 1;
  }
  printf("initialized data dir: %s\n", data_dir);
  return 0;
}

static int cmd_wallet_new(int argc, char **argv)
{
  char addr[CHIPPY_HEX_ADDR_LEN + 1];

  if (argc < 4) {
    return -1;
  }
  if (chippy_storage_wallet_new(data_dir, argv[3], addr) != 0) {
    fprintf(stderr, "wallet new failed\n");
    return 1;
  }
  printf("%s\n", addr);
  return 0;
}

static int make_signed_tx(chippy_tx *tx, const unsigned char *sk)
{
  unsigned char payload[512];
  unsigned char sig[crypto_sign_BYTES];
  size_t payload_len;

  if (chippy_tx_sign_payload(tx, payload, sizeof(payload), &payload_len) != 0) {
    return -1;
  }
  if (chippy_sign(sk, payload, payload_len, sig) != 0) {
    return -1;
  }
  return chippy_hex_encode(sig, sizeof(sig), tx->sig, sizeof(tx->sig));
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

static int cmd_mint(int argc, char **argv)
{
  chippy_chain chain;
  chippy_tx tx;
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  unsigned long long amount;

  if (argc < 4) {
    return -1;
  }
  if (strlen(argv[2]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  amount = strtoull(argv[3], NULL, 10);
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    fprintf(stderr, "load chain failed\n");
    return 1;
  }
  if (chippy_storage_mint_load_sec(data_dir, sk) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "load mint key failed\n");
    return 1;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, argv[2], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  if (make_signed_tx(&tx, sk) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "sign failed\n");
    return 1;
  }
  if (append_tx_block(&chain, &tx) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "append block failed\n");
    return 1;
  }
  chippy_chain_free(&chain);
  printf("minted %llu to %s\n", (unsigned long long)amount, argv[2]);
  return 0;
}

static int cmd_send(int argc, char **argv)
{
  chippy_chain chain;
  chippy_tx tx;
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  char from_addr[CHIPPY_HEX_ADDR_LEN + 1];
  unsigned long long amount;
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];

  if (argc < 5) {
    return -1;
  }
  if (strlen(argv[3]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid to address\n");
    return 1;
  }
  amount = strtoull(argv[4], NULL, 10);
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    fprintf(stderr, "load chain failed\n");
    return 1;
  }
  if (chippy_storage_wallet_load_sec(data_dir, argv[2], sk) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "load wallet failed\n");
    return 1;
  }
  crypto_sign_ed25519_sk_to_pk(pk, sk);
  if (chippy_pubkey_to_address(pk, from_addr) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "derive address failed\n");
    return 1;
  }
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, from_addr, CHIPPY_HEX_ADDR_LEN);
  tx.from[CHIPPY_HEX_ADDR_LEN] = '\0';
  strncpy(tx.to, argv[3], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  if (make_signed_tx(&tx, sk) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "sign failed\n");
    return 1;
  }
  if (append_tx_block(&chain, &tx) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "append block failed (insufficient balance?)\n");
    return 1;
  }
  chippy_chain_free(&chain);
  printf("sent %llu from %s to %s\n", (unsigned long long)amount, from_addr, argv[3]);
  return 0;
}

static int cmd_balance(int argc, char **argv)
{
  chippy_chain chain;
  uint64_t balance;

  if (argc < 3) {
    return -1;
  }
  if (strlen(argv[2]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    fprintf(stderr, "load chain failed\n");
    return 1;
  }
  if (chippy_chain_balance(&chain, argv[2], &balance) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "balance failed\n");
    return 1;
  }
  chippy_chain_free(&chain);
  printf("%llu\n", (unsigned long long)balance);
  return 0;
}

static int cmd_validate(int argc, char **argv)
{
  chippy_chain chain;

  (void)argc;
  (void)argv;
  chippy_chain_init(&chain);
  if (chippy_storage_load_chain(data_dir, &chain) != 0) {
    fprintf(stderr, "load chain failed\n");
    return 1;
  }
  if (chippy_chain_validate(&chain) != 0) {
    chippy_chain_free(&chain);
    fprintf(stderr, "chain invalid\n");
    return 1;
  }
  chippy_chain_free(&chain);
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
  } else if (strcmp(argv[1], "wallet") == 0 && argc >= 3 && strcmp(argv[2], "new") == 0) {
    rc = cmd_wallet_new(argc, argv);
  } else if (strcmp(argv[1], "mint") == 0) {
    rc = cmd_mint(argc, argv);
  } else if (strcmp(argv[1], "send") == 0) {
    rc = cmd_send(argc, argv);
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
