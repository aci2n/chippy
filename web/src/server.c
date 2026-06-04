#include "server.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define REQ_BUF_SIZE 65536
#define FILE_BUF_SIZE 65536
#define CONFIG_JS_MAX 512

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

static int path_safe(const char *path)
{
  if (path == NULL || path[0] != '/' || strstr(path, "..") != NULL) {
    return 0;
  }
  return 1;
}

static int js_escape(const char *in, char *out, size_t out_cap)
{
  size_t j;
  size_t i;

  if (in == NULL || out == NULL || out_cap < 2) {
    return -1;
  }
  j = 0;
  for (i = 0; in[i] != '\0'; i++) {
    if (j + 3 >= out_cap) {
      return -1;
    }
    if (in[i] == '\\' || in[i] == '"') {
      out[j++] = '\\';
    }
    out[j++] = in[i];
  }
  if (j + 1 >= out_cap) {
    return -1;
  }
  out[j] = '\0';
  return 0;
}

static int build_config_js(const char *api_url, char *body, size_t body_cap)
{
  char escaped[256];

  if (api_url == NULL) {
    return -1;
  }
  if (js_escape(api_url, escaped, sizeof(escaped)) != 0) {
    return -1;
  }
  if (snprintf(body, body_cap, "window.CHIPPY_API_URL=\"%s\";\n", escaped) < 0) {
    return -1;
  }
  return 0;
}

static const char *mime_for_path(const char *path)
{
  size_t n;

  n = strlen(path);
  if (n >= 5 && strcmp(path + n - 5, ".html") == 0) {
    return "text/html; charset=utf-8";
  }
  if (n >= 4 && strcmp(path + n - 4, ".css") == 0) {
    return "text/css; charset=utf-8";
  }
  if (n >= 3 && strcmp(path + n - 3, ".js") == 0) {
    return "application/javascript; charset=utf-8";
  }
  return "application/octet-stream";
}

static void send_http(int fd, int status, const char *status_text, const char *content_type,
                      const char *body, size_t body_len)
{
  char header[512];
  int n;

  n = snprintf(header, sizeof(header),
               "HTTP/1.1 %d %s\r\n"
               "Content-Type: %s\r\n"
               "Content-Length: %zu\r\n"
               "Connection: close\r\n\r\n",
               status, status_text, content_type, body_len);
  if (n > 0) {
    (void)write(fd, header, (size_t)n);
  }
  if (body_len > 0 && body != NULL) {
    (void)write(fd, body, body_len);
  }
}

static int read_file(const char *static_dir, const char *rel, char *buf, size_t buf_cap,
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

static void handle_client(int fd, const struct web_ctx *ctx)
{
  char req[REQ_BUF_SIZE];
  char file_buf[FILE_BUF_SIZE];
  char config_buf[CONFIG_JS_MAX];
  ssize_t nread;
  ssize_t hdr_end;
  char method[16];
  char path[256];
  const char *file_rel;
  size_t body_len;
  const char *mime;

  nread = read(fd, req, sizeof(req) - 1);
  if (nread <= 0) {
    return;
  }
  req[nread] = '\0';
  hdr_end = header_end_offset(req, (size_t)nread);
  if (hdr_end < 0) {
    send_http(fd, 400, "Bad Request", "text/plain", "bad request\n", 12);
    return;
  }

  if (sscanf(req, "%15s %255s", method, path) != 2) {
    send_http(fd, 400, "Bad Request", "text/plain", "bad request\n", 12);
    return;
  }
  if (strcmp(method, "GET") != 0) {
    send_http(fd, 405, "Method Not Allowed", "text/plain", "method not allowed\n", 19);
    return;
  }
  if (!path_safe(path)) {
    send_http(fd, 400, "Bad Request", "text/plain", "bad path\n", 9);
    return;
  }

  if (strcmp(path, "/config.js") == 0) {
    if (build_config_js(ctx->api_url, config_buf, sizeof(config_buf)) != 0) {
      send_http(fd, 500, "Internal Server Error", "text/plain", "config error\n", 13);
      return;
    }
    body_len = strlen(config_buf);
    send_http(fd, 200, "OK", "application/javascript; charset=utf-8", config_buf, body_len);
    return;
  }

  if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
    file_rel = "/index.html";
  } else if (strcmp(path, "/style.css") == 0) {
    file_rel = "/style.css";
  } else if (strcmp(path, "/app.js") == 0) {
    file_rel = "/app.js";
  } else {
    send_http(fd, 404, "Not Found", "text/plain", "not found\n", 10);
    return;
  }

  if (read_file(ctx->static_dir, file_rel, file_buf, sizeof(file_buf), &body_len) != 0) {
    send_http(fd, 404, "Not Found", "text/plain", "not found\n", 10);
    return;
  }
  mime = mime_for_path(file_rel);
  send_http(fd, 200, "OK", mime, file_buf, body_len);
}

int web_server_run(const struct web_ctx *ctx, const char *host, const char *port)
{
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *rp;
  int fd;
  int one;
  int err;
  char portbuf[16];

  if (ctx == NULL || host == NULL || port == NULL) {
    return -1;
  }
  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  snprintf(portbuf, sizeof(portbuf), "%s", port);
  err = getaddrinfo(host, portbuf, &hints, &res);
  if (err != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    return -1;
  }

  fd = -1;
  for (rp = res; rp != NULL; rp = rp->ai_next) {
    fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (fd < 0) {
      continue;
    }
    one = 1;
    (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    if (bind(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;
    }
    close(fd);
    fd = -1;
  }
  freeaddrinfo(res);
  if (fd < 0) {
    fprintf(stderr, "bind failed: %s\n", strerror(errno));
    return -1;
  }
  if (listen(fd, 16) != 0) {
    fprintf(stderr, "listen failed: %s\n", strerror(errno));
    close(fd);
    return -1;
  }

  for (;;) {
    int client = accept(fd, NULL, NULL);
    if (client < 0) {
      if (errno == EINTR) {
        continue;
      }
      fprintf(stderr, "accept: %s\n", strerror(errno));
      continue;
    }
    handle_client(client, ctx);
    close(client);
  }
}
