#include "cmd_local.h"
#include "cmd_serve.h"

#include <sodium.h>
#include <stdio.h>
#include <string.h>

typedef int (*chippy_cmd_fn)(int argc, char **argv);

typedef struct {
  const char *name;
  const char *sub; /* if set, argv[2] must match */
  chippy_cmd_fn run;
} chippy_command;

static const chippy_command g_commands[] = {
    {"serve", NULL, chippy_cmd_serve},
    {"init", NULL, cmd_init},
    {"keygen", NULL, cmd_keygen},
    {"sign-mint", NULL, cmd_sign_mint},
    {"sign-transfer", NULL, cmd_sign_transfer},
    {"mint", NULL, cmd_mint},
    {"transfer", NULL, cmd_transfer},
    {"balance", NULL, cmd_balance},
    {"mint-key", "add", cmd_mint_key_add},
    {"validate", NULL, cmd_validate},
};

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage:\n"
          "  %s serve [--dir .chippy] [--listen HOST:PORT]\n"
          "  %s init [--dir .chippy]\n"
          "  %s keygen\n"
          "  %s sign-mint <mint_secret> <mint_address> <to> <amount>\n"
          "  %s sign-transfer <secret_hex> <from> <to> <amount>\n"
          "  %s mint <to> <amount> <sig_hex> [--dir .chippy]\n"
          "  %s transfer <from> <to> <amount> <sig_hex> [--dir .chippy]\n"
          "  %s balance <addr> [--dir .chippy]\n"
          "  %s mint-key add <mint_address> [--dir .chippy]\n"
          "  %s validate [--dir .chippy]\n",
          prog, prog, prog, prog, prog, prog, prog, prog, prog, prog);
}

static int dispatch(int argc, char **argv)
{
  size_t i;
  int rc;

  for (i = 0; i < sizeof(g_commands) / sizeof(g_commands[0]); i++) {
    if (strcmp(argv[1], g_commands[i].name) != 0) {
      continue;
    }
    if (g_commands[i].sub != NULL) {
      if (argc < 3 || strcmp(argv[2], g_commands[i].sub) != 0) {
        continue;
      }
    }
    if (strcmp(g_commands[i].name, "serve") != 0) {
      chippy_apply_dir_flag(argc, argv);
    }
    rc = g_commands[i].run(argc, argv);
    if (rc < 0) {
      usage(argv[0]);
      return 1;
    }
    return rc;
  }
  usage(argv[0]);
  return 1;
}

int main(int argc, char **argv)
{
  if (sodium_init() < 0) {
    fprintf(stderr, "sodium_init failed\n");
    return 1;
  }

  if (argc < 2) {
    usage(argv[0]);
    return 1;
  }

  return dispatch(argc, argv);
}
