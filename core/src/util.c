#include "chippy.h"

#include <ctype.h>
#include <string.h>

static const char HEX_DIGITS[] = "0123456789abcdef";

int chippy_hex_encode(const unsigned char *bin, size_t bin_len, char *hex, size_t hex_cap)
{
  size_t need;

  if (bin == NULL || hex == NULL) {
    return -1;
  }
  need = bin_len * 2 + 1;
  if (hex_cap < need) {
    return -1;
  }
  for (size_t i = 0; i < bin_len; i++) {
    hex[i * 2] = HEX_DIGITS[(bin[i] >> 4) & 0x0f];
    hex[i * 2 + 1] = HEX_DIGITS[bin[i] & 0x0f];
  }
  hex[bin_len * 2] = '\0';
  return 0;
}

int chippy_hex_decode(const char *hex, unsigned char *bin, size_t bin_cap)
{
  size_t len;
  unsigned char hi;
  unsigned char lo;

  if (hex == NULL || bin == NULL) {
    return -1;
  }
  len = strlen(hex);
  if (len % 2 != 0 || len / 2 > bin_cap) {
    return -1;
  }
  for (size_t i = 0; i < len; i += 2) {
    if (!isxdigit((unsigned char)hex[i]) || !isxdigit((unsigned char)hex[i + 1])) {
      return -1;
    }
    hi = (unsigned char)hex[i];
    lo = (unsigned char)hex[i + 1];
    if (hi >= 'a') {
      hi = (unsigned char)(hi - 'a' + 10);
    } else if (hi >= 'A') {
      hi = (unsigned char)(hi - 'A' + 10);
    } else {
      hi = (unsigned char)(hi - '0');
    }
    if (lo >= 'a') {
      lo = (unsigned char)(lo - 'a' + 10);
    } else if (lo >= 'A') {
      lo = (unsigned char)(lo - 'A' + 10);
    } else {
      lo = (unsigned char)(lo - '0');
    }
    bin[i / 2] = (unsigned char)((hi << 4) | lo);
  }
  return (int)(len / 2);
}

int chippy_str_eq(const char *a, const char *b)
{
  if (a == NULL || b == NULL) {
    return 0;
  }
  return strcmp(a, b) == 0;
}
