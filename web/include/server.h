#ifndef WEB_SERVER_H
#define WEB_SERVER_H

struct web_ctx {
  const char *static_dir;
  const char *api_url;
};

int web_server_run(const struct web_ctx *ctx, const char *host, const char *port);

#endif /* WEB_SERVER_H */
