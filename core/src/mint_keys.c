#include "chippy.h"

#include <string.h>

void chippy_mint_keys_init(chippy_mint_keys *keys)
{
  if (keys == NULL) {
    return;
  }
  keys->count = 0;
}

int chippy_mint_keys_add(chippy_mint_keys *keys, const char *addr_hex)
{
  size_t i;

  if (keys == NULL || addr_hex == NULL) {
    return -1;
  }
  if (strlen(addr_hex) != CHIPPY_HEX_ADDR_LEN) {
    return -1;
  }
  if (keys->count >= CHIPPY_MAX_MINT_KEYS) {
    return -1;
  }
  for (i = 0; i < keys->count; i++) {
    if (chippy_str_eq(keys->keys[i], addr_hex)) {
      return 0;
    }
  }
  strncpy(keys->keys[keys->count], addr_hex, CHIPPY_HEX_ADDR_LEN);
  keys->keys[keys->count][CHIPPY_HEX_ADDR_LEN] = '\0';
  keys->count++;
  return 0;
}

int chippy_mint_keys_contains(const chippy_mint_keys *keys, const char *addr_hex)
{
  size_t i;

  if (keys == NULL || addr_hex == NULL) {
    return 0;
  }
  for (i = 0; i < keys->count; i++) {
    if (chippy_str_eq(keys->keys[i], addr_hex)) {
      return 1;
    }
  }
  return 0;
}

void chippy_mint_keys_copy(chippy_mint_keys *dst, const chippy_mint_keys *src)
{
  if (dst == NULL || src == NULL) {
    return;
  }
  *dst = *src;
}
