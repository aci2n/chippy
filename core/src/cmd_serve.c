#include "cmd_serve.h"

#include "chippy.h"
#include "http_server.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef CHIPPY_STATIC_DIR
#define CHIPPY_STATIC_DIR "static"
#endif

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

static void usage(const char *prog)
{
  fprintf(stderr, "usage: %s serve [--dir .chippy] [--listen HOST:PORT]\n", prog);
}

int chippy_cmd_serve(int argc, char **argv)
{
  struct serve_ctx ctx;
  char host[256];
  char port[16];
  const char *listen;
  int i;

  ctx.data_dir = CHIPPY_DEFAULT_DIR;
  ctx.static_dir = CHIPPY_STATIC_DIR;

  strncpy(host, "127.0.0.1", sizeof(host) - 1);
  host[sizeof(host) - 1] = '\0';
  strncpy(port, "8080", sizeof(port) - 1);
  port[sizeof(port) - 1] = '\0';

  listen = getenv("CHIPPY_LISTEN");
  if (listen != NULL && listen[0] != '\0') {
    if (parse_listen(listen, host, sizeof(host), port, sizeof(port)) != 0) {
      fprintf(stderr, "invalid CHIPPY_LISTEN\n");
      return 1;
    }
  }

  LOG_DEBUG("serve starting static_dir=%s", ctx.static_dir);
  for (i = 2; i < argc; i++) {
    if (strcmp(argv[i], "--dir") == 0 && i + 1 < argc) {
      ctx.data_dir = argv[++i];
      LOG_DEBUG("serve data_dir=%s", ctx.data_dir);
    } else if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
      if (parse_listen(argv[++i], host, sizeof(host), port, sizeof(port)) != 0) {
        usage(argv[0]);
        return 1;
      }
    } else if (strcmp(argv[i], "--static-dir") == 0 && i + 1 < argc) {
      ctx.static_dir = argv[++i];
    } else {
      usage(argv[0]);
      return 1;
    }
  }

  LOG_DEBUG("serve listen %s:%s data_dir=%s", host, port, ctx.data_dir);
  if (chippy_http_serve(&ctx, host, port) != 0) {
    LOG_DEBUG("serve failed");
    return 1;
  }
  return 0;
}
