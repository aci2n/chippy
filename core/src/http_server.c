#include "http_server.h"

#include "http_api.h"
#include "log.h"

#include "picohttpparser.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

#define REQ_BUF_SIZE 65536
#define FILE_BUF_SIZE 65536
#define MAX_HEADERS 32

enum route_kind {
  ROUTE_NONE = 0,
  ROUTE_API = 1,
  ROUTE_STATIC = 2,
};

struct route_result {
  int kind;
  struct http_response api_resp;
  char static_body[FILE_BUF_SIZE];
  size_t static_len;
  char static_content_type[64];
};

static ssize_t header_end_offset(const char *buf, size_t len)
{
  const char *p;
  size_t i;

  if (len < 4) {
    return -1;
  }
  for (i = 0; i + 3 < len; i++) {
    if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
      return (ssize_t)(i + 4);
    }
  }
  p = strstr(buf, "\r\n\r\n");
  if (p != NULL) {
    return (ssize_t)((p - buf) + 4);
  }
  return -1;
}

static int content_length_from_headers(const struct phr_header *headers, size_t num_headers)
{
  size_t i;

  for (i = 0; i < num_headers; i++) {
    if (headers[i].name_len == 14 &&
        strncasecmp(headers[i].name, "Content-Length", 14) == 0) {
      return atoi(headers[i].value);
    }
  }
  return 0;
}

static const char *status_text(int code)
{
  switch (code) {
  case 200:
    return "OK";
  case 204:
    return "No Content";
  case 400:
    return "Bad Request";
  case 404:
    return "Not Found";
  case 405:
    return "Method Not Allowed";
  case 409:
    return "Conflict";
  case 500:
    return "Internal Server Error";
  default:
    return "Error";
  }
}

static int path_safe(const char *path, size_t path_len)
{
  size_t i;

  if (path == NULL || path_len == 0 || path[0] != '/') {
    return 0;
  }
  for (i = 0; i + 1 < path_len; i++) {
    if (path[i] == '.' && path[i + 1] == '.') {
      return 0;
    }
  }
  return 1;
}

static int is_api_path(const char *path, size_t path_len)
{
  if (path_len == 7 && memcmp(path, "/health", 7) == 0) {
    return 1;
  }
  if (path_len >= 5 && memcmp(path, "/api/", 5) == 0) {
    return 1;
  }
  return 0;
}

static int read_static_file(const char *static_dir, const char *rel, char *buf, size_t buf_cap,
                            size_t *out_len)
{
  char path[1024];
  FILE *f;
  size_t n;

  if (snprintf(path, sizeof(path), "%s%s", static_dir, rel) >= (int)sizeof(path)) {
    return -1;
  }
  f = fopen(path, "rb");
  if (f == NULL) {
    return -1;
  }
  n = fread(buf, 1, buf_cap - 1, f);
  fclose(f);
  if (n >= buf_cap - 1) {
    return -1;
  }
  buf[n] = '\0';
  *out_len = n;
  return 0;
}

static const char *mime_for_rel(const char *rel)
{
  size_t n;

  n = strlen(rel);
  if (n >= 5 && strcmp(rel + n - 5, ".html") == 0) {
    return "text/html; charset=utf-8";
  }
  if (n >= 4 && strcmp(rel + n - 4, ".css") == 0) {
    return "text/css; charset=utf-8";
  }
  if (n >= 3 && strcmp(rel + n - 3, ".js") == 0) {
    return "application/javascript; charset=utf-8";
  }
  return "application/octet-stream";
}

static void send_raw(int fd, int status, const char *content_type, const char *body, size_t body_len)
{
  char header[640];
  int n;
  const char *text;

  text = status_text(status);
  if (status == 204) {
    n = snprintf(header, sizeof(header),
                 "HTTP/1.1 204 No Content\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 "Access-Control-Allow-Headers: Content-Type\r\n"
                 "Connection: close\r\n\r\n");
    if (n > 0) {
      (void)write(fd, header, (size_t)n);
    }
    return;
  }
  n = snprintf(header, sizeof(header),
               "HTTP/1.1 %d %s\r\n"
               "Content-Type: %s\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type\r\n"
               "Content-Length: %zu\r\n"
               "Connection: close\r\n\r\n",
               status, text, content_type, body_len);
  if (n > 0) {
    (void)write(fd, header, (size_t)n);
  }
  if (body_len > 0 && body != NULL) {
    (void)write(fd, body, body_len);
  }
}

static int handle_static_get(const struct serve_ctx *ctx, const char *path, size_t path_len,
                             struct route_result *out)
{
  const char *rel;

  if (!path_safe(path, path_len)) {
    return -1;
  }
  if (path_len == 1 && path[0] == '/') {
    rel = "/index.html";
  } else if (path_len == 11 && memcmp(path, "/index.html", 11) == 0) {
    rel = "/index.html";
  } else if (path_len == 10 && memcmp(path, "/style.css", 10) == 0) {
    rel = "/style.css";
  } else if (path_len == 7 && memcmp(path, "/app.js", 7) == 0) {
    rel = "/app.js";
  } else if (path_len == 10 && memcmp(path, "/crypto.js", 10) == 0) {
    rel = "/crypto.js";
  } else if (path_len == 10 && memcmp(path, "/config.js", 10) == 0) {
    snprintf(out->static_body, sizeof(out->static_body), "window.CHIPPY_API_URL=\"\";\n");
    out->static_len = strlen(out->static_body);
    snprintf(out->static_content_type, sizeof(out->static_content_type),
             "application/javascript; charset=utf-8");
    out->kind = ROUTE_STATIC;
    return 0;
  } else {
    return -1;
  }

  if (read_static_file(ctx->static_dir, rel, out->static_body, sizeof(out->static_body),
                       &out->static_len) != 0) {
    LOG_DEBUG("static file missing rel=%s dir=%s", rel, ctx->static_dir);
    return -1;
  }
  LOG_DEBUG("static file rel=%s len=%zu", rel, out->static_len);
  snprintf(out->static_content_type, sizeof(out->static_content_type), "%s", mime_for_rel(rel));
  out->kind = ROUTE_STATIC;
  return 0;
}

static void dispatch_request(const struct serve_ctx *ctx, const char *method, size_t method_len,
                             const char *path, size_t path_len, const char *body, size_t body_len,
                             struct route_result *result)
{
  struct api_ctx api;

  memset(result, 0, sizeof(*result));

  if (method_len == 7 && memcmp(method, "OPTIONS", 7) == 0) {
    result->kind = ROUTE_API;
    result->api_resp.status = 204;
    snprintf(result->api_resp.content_type, sizeof(result->api_resp.content_type),
             "application/json");
    result->api_resp.body[0] = '\0';
    return;
  }

  if (is_api_path(path, path_len)) {
    api.data_dir = ctx->data_dir;
    http_api_handle(&api, method, method_len, path, path_len, body, body_len, &result->api_resp);
    result->kind = ROUTE_API;
    return;
  }

  if (method_len == 3 && memcmp(method, "GET", 3) == 0) {
    if (handle_static_get(ctx, path, path_len, result) == 0) {
      return;
    }
  }
}

static void handle_client(int fd, const struct serve_ctx *ctx)
{
  char buf[REQ_BUF_SIZE];
  ssize_t nread;
  size_t total;
  ssize_t header_end;
  const char *method;
  size_t method_len;
  const char *path;
  size_t path_len;
  int minor_version;
  struct phr_header headers[MAX_HEADERS];
  size_t num_headers;
  int pret;
  int body_len;
  struct route_result result;
  char *body;

  total = 0;
  header_end = -1;
  while (total < sizeof(buf) && header_end < 0) {
    nread = read(fd, buf + total, sizeof(buf) - total);
    if (nread < 0) {
      if (errno == EINTR) {
        continue;
      }
      return;
    }
    if (nread == 0) {
      return;
    }
    total += (size_t)nread;
    header_end = header_end_offset(buf, total);
  }

  if (header_end < 0) {
    send_raw(fd, 400, "application/json", "{\"error\":\"bad request\"}", 21);
    return;
  }

  num_headers = MAX_HEADERS;
  pret = phr_parse_request(buf, (size_t)header_end, &method, &method_len, &path, &path_len,
                          &minor_version, headers, &num_headers, 0);
  if (pret < 0) {
    send_raw(fd, 400, "application/json", "{\"error\":\"bad request\"}", 21);
    return;
  }

  body_len = content_length_from_headers(headers, num_headers);
  while (total < (size_t)header_end + (size_t)body_len) {
    nread = read(fd, buf + total, sizeof(buf) - total);
    if (nread <= 0) {
      return;
    }
    total += (size_t)nread;
  }

  body = buf + header_end;
  LOG_DEBUG("http request %.*s %.*s body_len=%d", (int)method_len, method, (int)path_len, path,
            body_len);
  dispatch_request(ctx, method, method_len, path, path_len, body, (size_t)body_len, &result);
  (void)minor_version;

  if (result.kind == ROUTE_API) {
    LOG_DEBUG("http response api status=%d", result.api_resp.status);
    send_raw(fd, result.api_resp.status, result.api_resp.content_type, result.api_resp.body,
             strlen(result.api_resp.body));
    return;
  }
  if (result.kind == ROUTE_STATIC) {
    send_raw(fd, 200, result.static_content_type, result.static_body, result.static_len);
    return;
  }

  LOG_DEBUG("http 404 %.*s", (int)path_len, path);
  send_raw(fd, 404, "application/json", "{\"error\":\"not found\"}", 21);
}

int chippy_http_serve(const struct serve_ctx *ctx, const char *host, const char *port)
{
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *rp;
  int listen_fd;
  int client_fd;
  int yes;
  int gai;

  if (ctx == NULL || host == NULL || port == NULL) {
    return -1;
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;

  gai = getaddrinfo(host, port, &hints, &res);
  if (gai != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(gai));
    return -1;
  }

  listen_fd = -1;
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    listen_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (listen_fd < 0) {
      continue;
    }
    yes = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
    if (bind(listen_fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    close(listen_fd);
    listen_fd = -1;
  }
  freeaddrinfo(res);
  if (listen_fd < 0) {
    fprintf(stderr, "bind failed\n");
    return -1;
  }
  if (listen(listen_fd, 16) != 0) {
    perror("listen");
    close(listen_fd);
    return -1;
  }

  fprintf(stderr, "chippy serve: http://%s:%s/ (data dir %s)\n", host, port, ctx->data_dir);
  LOG_DEBUG("http server listening host=%s port=%s data_dir=%s static_dir=%s", host, port,
            ctx->data_dir, ctx->static_dir);

  for (;;) {
    client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("accept");
      continue;
    }
    LOG_DEBUG("http client connected fd=%d", client_fd);
    handle_client(client_fd, ctx);
    close(client_fd);
    LOG_DEBUG("http client closed fd=%d", client_fd);
  }
}
