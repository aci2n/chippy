#ifndef CHIPPY_HTTP_SERVER_H
#define CHIPPY_HTTP_SERVER_H

struct serve_ctx {
  const char *data_dir;
  const char *static_dir;
};

int chippy_http_serve(const struct serve_ctx *ctx, const char *host, const char *port);

#endif /* CHIPPY_HTTP_SERVER_H */
