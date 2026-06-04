#include "api.h"

#include "chippy_ops.h"
#include "json_util.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#define API_PREFIX "/api/v1"

static int path_eq(const char *path, size_t path_len, const char *literal)
{
  size_t n;

  if (path == NULL || literal == NULL) {
    return 0;
  }
  n = strlen(literal);
  return path_len == n && memcmp(path, literal, n) == 0;
}

static int path_prefix(const char *path, size_t path_len, const char *prefix, size_t *rest_off)
{
  size_t n;

  if (path == NULL || prefix == NULL) {
    return 0;
  }
  n = strlen(prefix);
  if (path_len < n || memcmp(path, prefix, n) != 0) {
    return 0;
  }
  if (rest_off != NULL) {
    *rest_off = n;
  }
  return 1;
}

static void resp_json(struct http_response *resp, int status, const char *json)
{
  resp->status = status;
  snprintf(resp->body, sizeof(resp->body), "%s", json);
}

static int mint_authorized(struct api_ctx *ctx, const char *headers, size_t headers_len)
{
  const char *needle = "X-Chippy-Mint-Token:";
  const char *p;
  const char *end;
  size_t token_len;

  if (ctx->mint_token == NULL || ctx->mint_token[0] == '\0') {
    return 1;
  }
  if (headers == NULL) {
    return 0;
  }
  end = headers + headers_len;
  p = headers;
  while (p < end) {
    const char *line_end = memchr(p, '\n', (size_t)(end - p));
    if (line_end == NULL) {
      line_end = end;
    }
    if ((size_t)(line_end - p) >= strlen(needle) && strncasecmp(p, needle, strlen(needle)) == 0) {
      const char *val = p + strlen(needle);
      const char *val_end = line_end;
      while (val < val_end && (*val == ' ' || *val == '\t')) {
        val++;
      }
      while (val_end > val && (val_end[-1] == '\r' || val_end[-1] == ' ' || val_end[-1] == '\t')) {
        val_end--;
      }
      token_len = strlen(ctx->mint_token);
      if ((size_t)(val_end - val) == token_len && memcmp(val, ctx->mint_token, token_len) == 0) {
        return 1;
      }
      return 0;
    }
    p = line_end + 1;
  }
  return 0;
}

void api_handle(struct api_ctx *ctx, const char *method, size_t method_len, const char *path,
                size_t path_len, const char *headers, size_t headers_len, const char *body,
                size_t body_len, struct http_response *resp)
{
  size_t off;
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  char from[CHIPPY_HEX_ADDR_LEN + 1];
  char to[CHIPPY_HEX_ADDR_LEN + 1];
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  chippy_tx tx;
  uint64_t amount;
  uint64_t balance;
  int rc;

  (void)body_len;
  memset(resp, 0, sizeof(*resp));
  resp_json(resp, 404, "{\"error\":\"not found\"}");

  if (path_eq(path, path_len, "/health")) {
    if (method_len == 3 && memcmp(method, "GET", 3) == 0) {
      resp_json(resp, 200, "{\"ok\":true}");
    }
    return;
  }

  if (method_len == 7 && memcmp(method, "OPTIONS", 7) == 0) {
    resp_json(resp, 204, "");
    return;
  }

  if (path_prefix(path, path_len, API_PREFIX "/balance/", &off)) {
    if (method_len != 3 || memcmp(method, "GET", 3) != 0) {
      return;
    }
    if (path_len - off != CHIPPY_HEX_ADDR_LEN) {
      resp_json(resp, 400, "{\"error\":\"invalid address\"}");
      return;
    }
    memcpy(addr, path + off, CHIPPY_HEX_ADDR_LEN);
    addr[CHIPPY_HEX_ADDR_LEN] = '\0';
    rc = chippy_op_balance(ctx->data_dir, addr, &balance);
    if (rc != 0) {
      resp_json(resp, 500, "{\"error\":\"balance failed\"}");
      return;
    }
    snprintf(resp->body, sizeof(resp->body), "{\"address\":\"%s\",\"balance\":%llu}", addr,
             (unsigned long long)balance);
    resp->status = 200;
    return;
  }

  if (path_eq(path, path_len, API_PREFIX "/chain/validate")) {
    if (method_len != 3 || memcmp(method, "GET", 3) != 0) {
      return;
    }
    rc = chippy_op_validate(ctx->data_dir);
    if (rc != 0) {
      resp_json(resp, 409, "{\"error\":\"chain invalid\"}");
      return;
    }
    resp_json(resp, 200, "{\"ok\":true}");
    return;
  }

  if (path_eq(path, path_len, API_PREFIX "/transfer")) {
    if (method_len != 4 || memcmp(method, "POST", 4) != 0) {
      return;
    }
    if (body == NULL || json_get_string(body, "from", from, sizeof(from)) != 0 ||
        json_get_string(body, "to", to, sizeof(to)) != 0 ||
        json_get_string(body, "sig", sig, sizeof(sig)) != 0 ||
        json_get_u64(body, "amount", &amount) != 0) {
      resp_json(resp, 400, "{\"error\":\"invalid json body\"}");
      return;
    }
    memset(&tx, 0, sizeof(tx));
    tx.type = CHIPPY_TX_TRANSFER;
    strncpy(tx.from, from, CHIPPY_HEX_ADDR_LEN);
    strncpy(tx.to, to, CHIPPY_HEX_ADDR_LEN);
    tx.amount = amount;
    strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
    rc = chippy_op_transfer(ctx->data_dir, &tx);
    if (rc != 0) {
      resp_json(resp, 409, "{\"error\":\"transfer rejected\"}");
      return;
    }
    resp_json(resp, 200, "{\"ok\":true}");
    return;
  }

  if (path_eq(path, path_len, API_PREFIX "/mint")) {
    if (method_len != 4 || memcmp(method, "POST", 4) != 0) {
      return;
    }
    if (!mint_authorized(ctx, headers, headers_len)) {
      resp_json(resp, 403, "{\"error\":\"mint not authorized\"}");
      return;
    }
    if (body == NULL || json_get_string(body, "to", to, sizeof(to)) != 0 ||
        json_get_u64(body, "amount", &amount) != 0) {
      resp_json(resp, 400, "{\"error\":\"invalid json body\"}");
      return;
    }
    rc = chippy_op_mint(ctx->data_dir, to, amount);
    if (rc != 0) {
      resp_json(resp, 409, "{\"error\":\"mint failed\"}");
      return;
    }
    resp_json(resp, 200, "{\"ok\":true}");
    return;
  }
}
