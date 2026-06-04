#ifndef JSON_UTIL_H
#define JSON_UTIL_H

#include <stddef.h>
#include <stdint.h>

/* extract JSON string value for "key"; unescapes minimal subset; returns 0 on success */
int json_get_string(const char *body, const char *key, char *out, size_t out_cap);

/* extract unsigned integer field; returns 0 on success */
int json_get_u64(const char *body, const char *key, uint64_t *out);

#endif /* JSON_UTIL_H */
