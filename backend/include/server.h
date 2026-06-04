#ifndef BACKEND_SERVER_H
#define BACKEND_SERVER_H

#include "api.h"

/* blocks until listen socket closes (never, on success) */
int http_server_run(struct api_ctx *ctx, const char *host, const char *port);

#endif /* BACKEND_SERVER_H */
