#include "chippy_ops.h"

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *data_dir = CHIPPY_DEFAULT_DIR;

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

static int cmd_init(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  if (chippy_op_init(data_dir) != 0) {
    fprintf(stderr, "init failed\n");
    return 1;
  }
  printf("initialized data dir: %s\n", data_dir);
  return 0;
}

static int cmd_keygen(void)
{
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  char sec[CHIPPY_HEX_SEC_LEN + 1];

  if (chippy_op_keygen(addr, sec) != 0) {
    fprintf(stderr, "keygen failed\n");
    return 1;
  }
  printf("address=%s\nsecret=%s\n", addr, sec);
  return 0;
}

static int cmd_sign_transfer(int argc, char **argv)
{
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  unsigned long long amount;

  if (argc < 6) {
    return -1;
  }
  amount = strtoull(argv[5], NULL, 10);
  if (chippy_op_sign_transfer(argv[2], argv[3], argv[4], (uint64_t)amount, sig) != 0) {
    fprintf(stderr, "sign-transfer failed\n");
    return 1;
  }
  printf("%s\n", sig);
  return 0;
}

static int cmd_transfer(int argc, char **argv)
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
  if (chippy_op_transfer(data_dir, &tx) != 0) {
    fprintf(stderr, "transfer failed\n");
    return 1;
  }
  printf("transferred %llu from %s to %s\n", (unsigned long long)amount, argv[2], argv[3]);
  return 0;
}

static int cmd_mint(int argc, char **argv)
{
  unsigned long long amount;

  if (argc < 4) {
    return -1;
  }
  if (strlen(argv[2]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  amount = strtoull(argv[3], NULL, 10);
  if (chippy_op_mint(data_dir, argv[2], (uint64_t)amount) != 0) {
    fprintf(stderr, "mint failed\n");
    return 1;
  }
  printf("minted %llu to %s\n", (unsigned long long)amount, argv[2]);
  return 0;
}

static int cmd_balance(int argc, char **argv)
{
  uint64_t balance;

  if (argc < 3) {
    return -1;
  }
  if (strlen(argv[2]) != CHIPPY_HEX_ADDR_LEN) {
    fprintf(stderr, "invalid address\n");
    return 1;
  }
  if (chippy_op_balance(data_dir, argv[2], &balance) != 0) {
    fprintf(stderr, "balance failed\n");
    return 1;
  }
  printf("%llu\n", (unsigned long long)balance);
  return 0;
}

static int cmd_validate(int argc, char **argv)
{
  (void)argc;
  (void)argv;
  if (chippy_op_validate(data_dir) != 0) {
    fprintf(stderr, "chain invalid\n");
    return 1;
  }
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
