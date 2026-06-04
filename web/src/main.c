#include "server.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_API_URL "http://127.0.0.1:8080"
#define DEFAULT_LISTEN "127.0.0.1:3000"

static void usage(const char *prog)
{
  fprintf(stderr,
          "usage: %s [--listen HOST:PORT] [--static-dir DIR]\n"
          "  CHIPPY_API_URL   backend base URL (default %s)\n"
          "  CHIPPY_LISTEN    same as --listen if set\n",
          prog, DEFAULT_API_URL);
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
  struct web_ctx ctx;
  char host[256];
  char port[16];
  const char *listen;
  int i;

#ifndef WEB_STATIC_DIR
#define WEB_STATIC_DIR "static"
#endif

  ctx.api_url = getenv("CHIPPY_API_URL");
  if (ctx.api_url == NULL || ctx.api_url[0] == '\0') {
    ctx.api_url = DEFAULT_API_URL;
  }
  ctx.static_dir = WEB_STATIC_DIR;

  strncpy(host, "127.0.0.1", sizeof(host) - 1);
  host[sizeof(host) - 1] = '\0';
  strncpy(port, "3000", sizeof(port) - 1);
  port[sizeof(port) - 1] = '\0';

  listen = getenv("CHIPPY_LISTEN");
  if (listen != NULL && listen[0] != '\0') {
    if (parse_listen(listen, host, sizeof(host), port, sizeof(port)) != 0) {
      fprintf(stderr, "invalid CHIPPY_LISTEN\n");
      return 1;
    }
  }

  for (i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--listen") == 0 && i + 1 < argc) {
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

  fprintf(stderr, "chippy web: static %s, api %s, listen %s:%s\n", ctx.static_dir, ctx.api_url,
          host, port);
  if (web_server_run(&ctx, host, port) != 0) {
    return 1;
  }
  return 0;
}
