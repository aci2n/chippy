#include "server.h"

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
#define MAX_HEADERS 32

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
  case 403:
    return "Forbidden";
  case 404:
    return "Not Found";
  case 409:
    return "Conflict";
  case 500:
    return "Internal Server Error";
  case 503:
    return "Service Unavailable";
  default:
    return "Error";
  }
}

static void send_response(int fd, const struct http_response *resp)
{
  char header[512];
  int n;
  const char *body;
  size_t body_len;
  const char *text;

  body = resp->body;
  body_len = strlen(body);
  text = status_text(resp->status);
  if (resp->status == 204) {
    n = snprintf(header, sizeof(header),
                 "HTTP/1.1 204 No Content\r\n"
                 "Access-Control-Allow-Origin: *\r\n"
                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                 "Access-Control-Allow-Headers: Content-Type, X-Chippy-Mint-Token\r\n"
                 "Connection: close\r\n\r\n");
    if (n > 0) {
      (void)write(fd, header, (size_t)n);
    }
    return;
  }
  n = snprintf(header, sizeof(header),
               "HTTP/1.1 %d %s\r\n"
               "Content-Type: application/json\r\n"
               "Access-Control-Allow-Origin: *\r\n"
               "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
               "Access-Control-Allow-Headers: Content-Type, X-Chippy-Mint-Token\r\n"
               "Content-Length: %zu\r\n"
               "Connection: close\r\n\r\n",
               resp->status, text, body_len);
  if (n > 0) {
    (void)write(fd, header, (size_t)n);
  }
  if (body_len > 0) {
    (void)write(fd, body, body_len);
  }
}

static void send_error(int fd, int code, const char *json)
{
  struct http_response resp;

  memset(&resp, 0, sizeof(resp));
  resp.status = code;
  snprintf(resp.body, sizeof(resp.body), "%s", json);
  send_response(fd, &resp);
}

static void handle_client(int fd, struct api_ctx *ctx)
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
  struct http_response resp;
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
    send_error(fd, 400, "{\"error\":\"bad request\"}");
    return;
  }

  num_headers = MAX_HEADERS;
  pret = phr_parse_request(buf, (size_t)header_end, &method, &method_len, &path, &path_len,
                          &minor_version, headers, &num_headers, 0);
  if (pret < 0) {
    send_error(fd, 400, "{\"error\":\"bad request\"}");
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
  api_handle(ctx, method, method_len, path, path_len, buf, (size_t)header_end, body,
             (size_t)body_len, &resp);
  send_response(fd, &resp);
  (void)minor_version;
}

int http_server_run(struct api_ctx *ctx, const char *host, const char *port)
{
  struct addrinfo hints;
  struct addrinfo *res;
  struct addrinfo *rp;
  int listen_fd;
  int client_fd;
  int yes;
  int gai;

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

  fprintf(stderr, "backend listening on %s:%s (data dir %s)\n", host, port, ctx->data_dir);

  for (;;) {
    client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
      if (errno == EINTR) {
        continue;
      }
      perror("accept");
      continue;
    }
    handle_client(client_fd, ctx);
    close(client_fd);
  }
}
