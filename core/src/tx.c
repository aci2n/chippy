#include "chippy.h"

#include <sodium.h>
#include <stdio.h>
#include <string.h>

int chippy_tx_sign_payload(const chippy_tx *tx, unsigned char *buf, size_t buf_cap,
                           size_t *out_len)
{
  int n;

  if (tx == NULL || buf == NULL || out_len == NULL) {
    return -1;
  }
  if (tx->type == CHIPPY_TX_MINT) {
    n = snprintf((char *)buf, buf_cap, "mint\n%s\n%llu", tx->to,
                   (unsigned long long)tx->amount);
  } else {
    n = snprintf((char *)buf, buf_cap, "transfer\n%s\n%s\n%llu", tx->from, tx->to,
                   (unsigned long long)tx->amount);
  }
  if (n < 0 || (size_t)n >= buf_cap) {
    return -1;
  }
  *out_len = (size_t)n;
  return 0;
}

int chippy_tx_sign(chippy_tx *tx, const unsigned char *sk)
{
  unsigned char payload[512];
  unsigned char sig[crypto_sign_BYTES];
  size_t payload_len;

  if (tx == NULL || sk == NULL) {
    return -1;
  }
  if (chippy_tx_sign_payload(tx, payload, sizeof(payload), &payload_len) != 0) {
    return -1;
  }
  if (chippy_sign(sk, payload, payload_len, sig) != 0) {
    return -1;
  }
  return chippy_hex_encode(sig, sizeof(sig), tx->sig, sizeof(tx->sig));
}

int chippy_tx_verify(const chippy_tx *tx, const char *mint_pubkey)
{
  unsigned char payload[512];
  unsigned char sig[crypto_sign_BYTES];
  size_t payload_len;
  const char *signer;

  if (tx == NULL || mint_pubkey == NULL) {
    return -1;
  }
  if (chippy_tx_sign_payload(tx, payload, sizeof(payload), &payload_len) != 0) {
    return -1;
  }
  if (strlen(tx->sig) != CHIPPY_HEX_SIG_LEN) {
    return -1;
  }
  if (chippy_hex_decode(tx->sig, sig, sizeof(sig)) != (int)crypto_sign_BYTES) {
    return -1;
  }
  if (tx->type == CHIPPY_TX_MINT) {
    signer = mint_pubkey;
  } else {
    if (strlen(tx->from) != CHIPPY_HEX_ADDR_LEN) {
      return -1;
    }
    signer = tx->from;
  }
  return chippy_verify(signer, payload, payload_len, sig);
}

int chippy_tx_parse_line(const char *line, chippy_tx *tx)
{
  char kind[16];
  char from[CHIPPY_HEX_ADDR_LEN + 1];
  char to[CHIPPY_HEX_ADDR_LEN + 1];
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  unsigned long long amount;
  int n;

  if (line == NULL || tx == NULL) {
    return -1;
  }
  memset(tx, 0, sizeof(*tx));
  n = sscanf(line, "%15s %64s %64s %llu %128s", kind, from, to, &amount, sig);
  if (n < 4) {
    return -1;
  }
  if (strcmp(kind, "mint") == 0) {
    tx->type = CHIPPY_TX_MINT;
    if (strcmp(from, "-") != 0) {
      return -1;
    }
    tx->from[0] = '\0';
    strncpy(tx->to, to, CHIPPY_HEX_ADDR_LEN);
    tx->to[CHIPPY_HEX_ADDR_LEN] = '\0';
    tx->amount = (uint64_t)amount;
    if (n >= 5) {
      strncpy(tx->sig, sig, CHIPPY_HEX_SIG_LEN);
      tx->sig[CHIPPY_HEX_SIG_LEN] = '\0';
    }
    return 0;
  }
  if (strcmp(kind, "transfer") == 0) {
    tx->type = CHIPPY_TX_TRANSFER;
    strncpy(tx->from, from, CHIPPY_HEX_ADDR_LEN);
    tx->from[CHIPPY_HEX_ADDR_LEN] = '\0';
    strncpy(tx->to, to, CHIPPY_HEX_ADDR_LEN);
    tx->to[CHIPPY_HEX_ADDR_LEN] = '\0';
    tx->amount = (uint64_t)amount;
    if (n >= 5) {
      strncpy(tx->sig, sig, CHIPPY_HEX_SIG_LEN);
      tx->sig[CHIPPY_HEX_SIG_LEN] = '\0';
    }
    return 0;
  }
  return -1;
}

int chippy_tx_format_line(const chippy_tx *tx, char *line, size_t line_cap)
{
  int n;

  if (tx == NULL || line == NULL) {
    return -1;
  }
  if (tx->type == CHIPPY_TX_MINT) {
    n = snprintf(line, line_cap, "mint - %s %llu %s", tx->to,
                 (unsigned long long)tx->amount, tx->sig);
  } else {
    n = snprintf(line, line_cap, "transfer %s %s %llu %s", tx->from, tx->to,
                 (unsigned long long)tx->amount, tx->sig);
  }
  if (n < 0 || (size_t)n >= line_cap) {
    return -1;
  }
  return 0;
}
