#ifndef CHIPPY_HTTP_API_H
#define CHIPPY_HTTP_API_H

#include <stddef.h>

struct api_ctx {
  const char *data_dir;
};

struct http_response {
  int status;
  char content_type[64];
  char body[4096];
};

void http_api_handle(struct api_ctx *ctx, const char *method, size_t method_len,
                     const char *path, size_t path_len, const char *body, size_t body_len,
                     struct http_response *resp);

#endif /* CHIPPY_HTTP_API_H */
