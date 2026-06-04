#include "json_util.h"
#include "log.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *find_key(const char *body, const char *key)
{
  char pattern[64];
  int n;

  if (body == NULL || key == NULL) {
    return NULL;
  }
  n = snprintf(pattern, sizeof(pattern), "\"%s\"", key);
  if (n < 0 || (size_t)n >= sizeof(pattern)) {
    return NULL;
  }
  return strstr(body, pattern);
}

int json_get_string(const char *body, const char *key, char *out, size_t out_cap)
{
  const char *p;
  const char *start;
  const char *end;
  size_t len;

  if (body == NULL || key == NULL || out == NULL || out_cap == 0) {
    return -1;
  }
  p = find_key(body, key);
  if (p == NULL) {
    return -1;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return -1;
  }
  p++;
  while (*p != '\0' && isspace((unsigned char)*p)) {
    p++;
  }
  if (*p != '"') {
    return -1;
  }
  start = p + 1;
  end = start;
  while (*end != '\0' && *end != '"') {
    if (*end == '\\' && end[1] != '\0') {
      end += 2;
      continue;
    }
    end++;
  }
  if (*end != '"') {
    return -1;
  }
  len = (size_t)(end - start);
  if (len + 1 > out_cap) {
    return -1;
  }
  memcpy(out, start, len);
  out[len] = '\0';
  LOG_DEBUG("json_get_string key=%s len=%zu", key, len);
  return 0;
}

int json_get_u64(const char *body, const char *key, uint64_t *out)
{
  const char *p;
  char *end;
  unsigned long long v;

  if (body == NULL || key == NULL || out == NULL) {
    return -1;
  }
  p = find_key(body, key);
  if (p == NULL) {
    return -1;
  }
  p = strchr(p, ':');
  if (p == NULL) {
    return -1;
  }
  p++;
  while (*p != '\0' && isspace((unsigned char)*p)) {
    p++;
  }
  v = strtoull(p, &end, 10);
  if (end == p) {
    return -1;
  }
  *out = (uint64_t)v;
  LOG_DEBUG("json_get_u64 key=%s value=%llu", key, (unsigned long long)*out);
  return 0;
}
