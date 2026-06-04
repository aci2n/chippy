#include "cmd_local.h"
#include "cmd_serve.h"

#include <sodium.h>
#include <stdio.h>
#include <string.h>

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage:\n"
          "  %s serve [--dir .chippy] [--listen HOST:PORT]\n"
          "  %s init | keygen | sign-mint | sign-transfer | mint | transfer |\n"
          "       balance | mint-key add | validate  [--dir .chippy]\n",
          prog, prog);
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

  if (strcmp(argv[1], "serve") == 0) {
    return chippy_cmd_serve(argc, argv);
  }

  return chippy_cmd_local(argc, argv);
}
