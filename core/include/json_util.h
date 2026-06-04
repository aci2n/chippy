#ifndef CHIPPY_JSON_UTIL_H
#define CHIPPY_JSON_UTIL_H

#include <stddef.h>
#include <stdint.h>

int json_get_string(const char *body, const char *key, char *out, size_t out_cap);
int json_get_u64(const char *body, const char *key, uint64_t *out);

#endif /* CHIPPY_JSON_UTIL_H */
