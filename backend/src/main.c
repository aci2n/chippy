#include "api.h"
#include "chippy.h"
#include "server.h"

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage: %s [--dir .chippy] [--listen HOST:PORT] [--mint-token TOKEN]\n"
          "  env CHIPPY_MINT_TOKEN also enables mint auth header\n",
          prog);
}

static int parse_listen(const char *arg, char *host, size_t host_cap, char *port, size_t port_cap)
{
  const char *colon;
  size_t host_len;

  colon = strrchr(arg, ':');
  if (colon == NULL || colon == arg) {
    return -1;
  }
  host_len = (size_t)(colon - arg);
  if (host_len + 1 > host_cap || strlen(colon + 1) + 1 > port_cap) {
    return -1;
  }
  memcpy(host, arg, host_len);
  host[host_len] = '\0';
  strncpy(port, colon + 1, port_cap - 1);
  port[port_cap - 1] = '\0';
  return 0;
}

int main(int argc, char **argv)
{
  struct api_ctx ctx;
  char host[256];
  char port[16];
  const char *mint_token;
  int i;

  if (sodium_init() < 0) {
    fprintf(stderr, "sodium_init failed\n");
    return 1;
  }

  ctx.data_dir = CHIPPY_DEFAULT_DIR;
  strncpy(host, "127.0.0.1", sizeof(host) - 1);
  strncpy(port, "8080", sizeof(port) - 1);
  mint_token = getenv("CHIPPY_MINT_TOKEN");

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
      ctx.data_dir = argv[++i];
    } else if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
      if (parse_listen(argv[++i], host, sizeof(host), port, sizeof(port)) != 0) {
        usage(argv[0]);
        return 1;
      }
    } else if (strcmp(argv[i], "--mint-token") == 0 && i + 1 < argc) {
      mint_token = argv[++i];
    } else {
      usage(argv[0]);
      return 1;
    }
  }

  ctx.mint_token = mint_token;
  if (http_server_run(&ctx, host, port) != 0) {
    return 1;
  }
  return 0;
}
