#include "cmd_local.h"

#include "chippy_ops.h"
#include "chippy.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *data_dir = CHIPPY_DEFAULT_DIR;

void chippy_apply_dir_flag(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc - 1; i++) {
    if (strcmp(argv[i], "--dir") == 0) {
      data_dir = argv[i + 1];
      LOG_DEBUG("data dir=%s", data_dir);
      return;
    }
  }
}

int cmd_init(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  LOG_DEBUG("init dir=%s", data_dir);
  if (chippy_op_init(data_dir) != 0) {
    LOG_DEBUG("init failed dir=%s", data_dir);
    fprintf(stderr, "init failed\n");
    return 1;
  }
  printf("initialized data dir: %s\n", data_dir);
  return 0;
}

int cmd_keygen(int argc, char **argv)
{
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  char sec[CHIPPY_HEX_SEC_LEN + 1];

  (void)argc;
  (void)argv;
  LOG_DEBUG("keygen");
  if (chippy_op_keygen(addr, sec) != 0) {
    LOG_DEBUG("keygen failed");
    fprintf(stderr, "keygen failed\n");
    return 1;
  }
  LOG_DEBUG("keygen ok address=%s", addr);
  printf("address=%s\nsecret=%s\n", addr, sec);
  return 0;
}

int cmd_sign_mint(int argc, char **argv)
{
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  unsigned long long amount;

  if (argc < 6) {
    return -1;
  }
  amount = strtoull(argv[5], NULL, 10);
  LOG_DEBUG("sign-mint mint_addr=%s to=%s amount=%llu", argv[3], argv[4],
            (unsigned long long)amount);
  if (chippy_op_sign_mint(argv[2], argv[3], argv[4], (uint64_t)amount, sig) != 0) {
    fprintf(stderr, "sign-mint failed\n");
    return 1;
  }
  printf("%s\n", sig);
  return 0;
}

int cmd_sign_transfer(int argc, char **argv)
{
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  unsigned long long amount;

  if (argc < 6) {
    return -1;
  }
  amount = strtoull(argv[5], NULL, 10);
  LOG_DEBUG("sign-transfer from=%s to=%s amount=%llu", argv[3], argv[4],
            (unsigned long long)amount);
  if (chippy_op_sign_transfer(argv[2], argv[3], argv[4], (uint64_t)amount, sig) != 0) {
    fprintf(stderr, "sign-transfer failed\n");
    return 1;
  }
  printf("%s\n", sig);
  return 0;
}

int cmd_mint(int argc, char **argv)
{
  chippy_tx tx;
  unsigned long long amount;

  if (argc < 5) {
    return -1;
  }
  if (strlen(argv[2]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  if (strlen(argv[4]) != CHIPPY_HEX_SIG_LEN) {
    fprintf(stderr, "invalid signature\n");
    return 1;
  }
  amount = strtoull(argv[3], NULL, 10);
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  tx.from[0] = '\0';
  strncpy(tx.to, argv[2], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  strncpy(tx.sig, argv[4], CHIPPY_HEX_SIG_LEN);
  tx.sig[CHIPPY_HEX_SIG_LEN] = '\0';
  LOG_DEBUG("mint dir=%s to=%s amount=%llu", data_dir, argv[2],
            (unsigned long long)amount);
  if (chippy_op_mint(data_dir, &tx) != 0) {
    LOG_DEBUG("mint failed to=%s", argv[2]);
    fprintf(stderr, "mint failed\n");
    return 1;
  }
  printf("minted %llu to %s\n", (unsigned long long)amount, argv[2]);
  return 0;
}

int cmd_transfer(int argc, char **argv)
{
  chippy_tx tx;
  unsigned long long amount;

  if (argc < 6) {
    return -1;
  }
  if (strlen(argv[5]) != CHIPPY_HEX_SIG_LEN) {
    fprintf(stderr, "invalid signature\n");
    return 1;
  }
  amount = strtoull(argv[4], NULL, 10);
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, argv[2], CHIPPY_HEX_ADDR_LEN);
  tx.from[CHIPPY_HEX_ADDR_LEN] = '\0';
  strncpy(tx.to, argv[3], CHIPPY_HEX_ADDR_LEN);
  tx.to[CHIPPY_HEX_ADDR_LEN] = '\0';
  tx.amount = (uint64_t)amount;
  strncpy(tx.sig, argv[5], CHIPPY_HEX_SIG_LEN);
  tx.sig[CHIPPY_HEX_SIG_LEN] = '\0';
  LOG_DEBUG("transfer dir=%s from=%s to=%s amount=%llu", data_dir, argv[2], argv[3],
            (unsigned long long)amount);
  if (chippy_op_transfer(data_dir, &tx) != 0) {
    LOG_DEBUG("transfer failed from=%s to=%s", argv[2], argv[3]);
    fprintf(stderr, "transfer failed\n");
    return 1;
  }
  printf("transferred %llu from %s to %s\n", (unsigned long long)amount, argv[2], argv[3]);
  return 0;
}

int cmd_balance(int argc, char **argv)
{
  uint64_t balance;

  if (argc < 3) {
    return -1;
  }
  if (strlen(argv[2]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  LOG_DEBUG("balance dir=%s addr=%s", data_dir, argv[2]);
  if (chippy_op_balance(data_dir, argv[2], &balance) != 0) {
    LOG_DEBUG("balance failed addr=%s", argv[2]);
    fprintf(stderr, "balance failed\n");
    return 1;
  }
  LOG_DEBUG("balance addr=%s value=%llu", argv[2], (unsigned long long)balance);
  printf("%llu\n", (unsigned long long)balance);
  return 0;
}

int cmd_mint_key_add(int argc, char **argv)
{
  if (argc < 4) {
    return -1;
  }
  if (strcmp(argv[2], "add") != 0) {
    return -1;
  }
  if (strlen(argv[3]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid mint address\n");
    return 1;
  }
  LOG_DEBUG("mint-key add dir=%s addr=%s", data_dir, argv[3]);
  if (chippy_dir_lock(data_dir) != 0) {
    LOG_DEBUG("mint-key add lock failed dir=%s", data_dir);
    fprintf(stderr, "data dir in use\n");
    return 1;
  }
  if (chippy_storage_add_authorized_mint_key(data_dir, argv[3]) != 0) {
    chippy_dir_unlock(data_dir);
    fprintf(stderr, "mint-key add failed\n");
    return 1;
  }
  chippy_dir_unlock(data_dir);
  LOG_DEBUG("mint-key add ok addr=%s", argv[3]);
  printf("authorized mint key %s\n", argv[3]);
  return 0;
}

int cmd_validate(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  LOG_DEBUG("validate dir=%s", data_dir);
  if (chippy_op_validate(data_dir) != 0) {
    LOG_DEBUG("validate failed dir=%s", data_dir);
    fprintf(stderr, "chain invalid\n");
    return 1;
  }
  LOG_DEBUG("validate ok dir=%s", data_dir);
  printf("ok\n");
  return 0;
}

