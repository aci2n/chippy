#include "chippy.h"
#include "log.h"

#include <sodium.h>
#include <string.h>

int chippy_keypair_generate(unsigned char *pk, unsigned char *sk)
{
  if (pk == NULL || sk == NULL) {
    return -1;
  }
  crypto_sign_keypair(pk, sk);
  LOG_DEBUG("keypair_generate");
  return 0;
}

int chippy_pubkey_to_address(const unsigned char *pk, char *addr_hex)
{
  return chippy_hex_encode(pk, crypto_sign_PUBLICKEYBYTES, addr_hex,
                          CHIPPY_HEX_ADDR_LEN + 1);
}

int chippy_address_to_pubkey(const char *addr_hex, unsigned char *pk)
{
  int n;

  if (addr_hex == NULL || pk == NULL) {
    return -1;
  }
  if (strlen(addr_hex) != CHIPPY_HEX_ADDR_LEN) {
    return -1;
  }
  n = chippy_hex_decode(addr_hex, pk, crypto_sign_PUBLICKEYBYTES);
  if (n != (int)crypto_sign_PUBLICKEYBYTES) {
    return -1;
  }
  return 0;
}

int chippy_sign(const unsigned char *sk, const unsigned char *msg, size_t msg_len,
                unsigned char *sig)
{
  unsigned long long sig_len;

  if (sk == NULL || msg == NULL || sig == NULL) {
    return -1;
  }
  sig_len = 0;
  if (crypto_sign_detached(sig, &sig_len, msg, msg_len, sk) != 0) {
    return -1;
  }
  return 0;
}

int chippy_verify(const char *addr_hex, const unsigned char *msg, size_t msg_len,
                  const unsigned char *sig)
{
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];

  if (addr_hex == NULL || msg == NULL || sig == NULL) {
    return -1;
  }
  if (chippy_address_to_pubkey(addr_hex, pk) != 0) {
    return -1;
  }
  if (crypto_sign_verify_detached(sig, msg, msg_len, pk) != 0) {
    LOG_DEBUG("verify failed addr=%s msg_len=%zu", addr_hex, msg_len);
    return -1;
  }
  return 0;
}

int chippy_sha256_hex(const unsigned char *data, size_t data_len, char *out_hex)
{
  unsigned char digest[crypto_hash_sha256_BYTES];

  if (data == NULL || out_hex == NULL) {
    return -1;
  }
  crypto_hash_sha256(digest, data, data_len);
  return chippy_hex_encode(digest, sizeof(digest), out_hex, CHIPPY_HEX_HASH_LEN + 1);
}
