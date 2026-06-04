#include "chippy.h"

#include <sodium.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int failures;
static int temp_serial;

static void fail(const char *msg)
{
  fprintf(stderr, "FAIL: %s\n", msg);
  failures++;
}

static void ok(const char *msg)
{
  fprintf(stdout, "ok %s\n", msg);
}

#define ASSERT(cond, msg) \
  do { \
    if (!(cond)) { \
      fail(msg); \
    } else { \
      ok(msg); \
    } \
  } while (0)

static int sign_tx(chippy_tx *tx, const unsigned char *sk)
{
  unsigned char payload[512];
  unsigned char sig[crypto_sign_BYTES];
  size_t payload_len;

  if (chippy_tx_sign_payload(tx, payload, sizeof(payload), &payload_len) != 0) {
    return -1;
  }
  if (chippy_sign(sk, payload, payload_len, sig) != 0) {
    return -1;
  }
  return chippy_hex_encode(sig, sizeof(sig), tx->sig, sizeof(tx->sig));
}

static int append_block(chippy_chain *chain, const chippy_tx *tx)
{
  chippy_block block;
  const chippy_block *tip;
  chippy_tx *heap_tx;
  time_t now;

  heap_tx = malloc(sizeof(chippy_tx));
  if (heap_tx == NULL) {
    return -1;
  }
  *heap_tx = *tx;
  memset(&block, 0, sizeof(block));
  block.txs = heap_tx;
  block.tx_count = 1;
  if (chain->block_count == 0) {
    free(heap_tx);
    return -1;
  }
  tip = &chain->blocks[chain->block_count - 1];
  block.index = tip->index + 1;
  now = time(NULL);
  block.timestamp = (uint64_t)now;
  strncpy(block.prev_hash, tip->hash, CHIPPY_HEX_HASH_LEN);
  block.prev_hash[CHIPPY_HEX_HASH_LEN] = '\0';
  if (chippy_block_compute_hash(&block, block.hash) != 0) {
    free(heap_tx);
    return -1;
  }
  return chippy_chain_append_block(chain, &block);
}

static char *make_temp_dir(void)
{
  char *dir;
  int n;

  dir = malloc(64);
  if (dir == NULL) {
    return NULL;
  }
  n = snprintf(dir, 64, "/tmp/chippy-test-%ld-%d-%d", (long)time(NULL), (int)getpid(),
               temp_serial++);
  if (n < 0 || n >= 64) {
    free(dir);
    return NULL;
  }
  if (mkdir(dir, 0700) != 0) {
    free(dir);
    return NULL;
  }
  return dir;
}

static void test_dir_lock(char *dir)
{
  ASSERT(chippy_dir_lock(dir) == 0, "dir lock");
  ASSERT(chippy_dir_lock(dir) == 0, "dir lock reentrant");
  ASSERT(chippy_dir_unlock(dir) == 0, "dir unlock inner");
  ASSERT(chippy_dir_unlock(dir) == 0, "dir unlock outer");
}

static void test_hex(void)
{
  unsigned char bin[] = {0xde, 0xad, 0xbe, 0xef};
  char hex[16];
  unsigned char out[8];

  ASSERT(chippy_hex_encode(bin, 4, hex, sizeof(hex)) == 0, "hex encode");
  ASSERT(strcmp(hex, "deadbeef") == 0, "hex encode value");
  ASSERT(chippy_hex_decode(hex, out, sizeof(out)) == 4, "hex decode len");
  ASSERT(memcmp(bin, out, 4) == 0, "hex roundtrip");
}

static void test_crypto(void)
{
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  unsigned char msg[] = "chippy test message";
  unsigned char sig[crypto_sign_BYTES];

  ASSERT(chippy_keypair_generate(pk, sk) == 0, "keypair generate");
  ASSERT(chippy_pubkey_to_address(pk, addr) == 0, "pubkey to address");
  ASSERT(strlen(addr) == CHIPPY_HEX_ADDR_LEN, "address length");
  ASSERT(chippy_sign(sk, msg, sizeof(msg) - 1, sig) == 0, "sign");
  ASSERT(chippy_verify(addr, msg, sizeof(msg) - 1, sig) == 0, "verify");
  ASSERT(chippy_verify(addr, msg, sizeof(msg) - 2, sig) != 0, "verify rejects tampered msg");
}

static void test_tx_format_parse(void)
{
  chippy_tx tx;
  chippy_tx parsed;
  char line[1024];

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
          CHIPPY_HEX_ADDR_LEN);
  tx.amount = 42;
  strncpy(tx.sig,
          "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"
          "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
          CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_tx_format_line(&tx, line, sizeof(line)) == 0, "tx format mint");
  ASSERT(chippy_tx_parse_line(line, &parsed) == 0, "tx parse mint");
  ASSERT(parsed.type == CHIPPY_TX_MINT, "parsed mint type");
  ASSERT(parsed.amount == 42, "parsed mint amount");
  ASSERT(chippy_str_eq(parsed.to, tx.to), "parsed mint to");
}

static void test_chain_flow(char *dir)
{
  chippy_chain chain;
  chippy_tx tx;
  unsigned char mint_sk[crypto_sign_SECRETKEYBYTES];
  unsigned char alice_pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char alice_sk[crypto_sign_SECRETKEYBYTES];
  unsigned char bob_pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char bob_sk[crypto_sign_SECRETKEYBYTES];
  char alice_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char bob_addr[CHIPPY_HEX_ADDR_LEN + 1];
  uint64_t balance;

  chippy_chain_init(&chain);
  ASSERT(chippy_storage_init(dir) == 0, "storage init");
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "load genesis");
  ASSERT(chippy_chain_validate(&chain) == 0, "genesis validates");
  ASSERT(chippy_storage_mint_load_sec(dir, mint_sk) == 0, "load mint key");

  ASSERT(chippy_keypair_generate(alice_pk, alice_sk) == 0, "alice keypair");
  ASSERT(chippy_pubkey_to_address(alice_pk, alice_addr) == 0, "alice address");
  ASSERT(chippy_keypair_generate(bob_pk, bob_sk) == 0, "bob keypair");
  ASSERT(chippy_pubkey_to_address(bob_pk, bob_addr) == 0, "bob address");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, alice_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 1000;
  ASSERT(sign_tx(&tx, mint_sk) == 0, "sign mint tx");
  ASSERT(append_block(&chain, &tx) == 0, "append mint block");
  ASSERT(chippy_chain_balance(&chain, alice_addr, &balance) == 0, "alice balance after mint");
  ASSERT(balance == 1000, "alice has 1000");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, alice_addr, CHIPPY_HEX_ADDR_LEN);
  strncpy(tx.to, bob_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 250;
  ASSERT(sign_tx(&tx, alice_sk) == 0, "sign transfer tx");
  ASSERT(append_block(&chain, &tx) == 0, "append transfer block");
  ASSERT(chippy_chain_balance(&chain, alice_addr, &balance) == 0, "alice balance after send");
  ASSERT(balance == 750, "alice has 750");
  ASSERT(chippy_chain_balance(&chain, bob_addr, &balance) == 0, "bob balance");
  ASSERT(balance == 250, "bob has 250");
  ASSERT(chippy_chain_validate(&chain) == 0, "chain validates");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, alice_addr, CHIPPY_HEX_ADDR_LEN);
  strncpy(tx.to, bob_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 9000;
  ASSERT(sign_tx(&tx, alice_sk) == 0, "sign overspend tx");
  ASSERT(append_block(&chain, &tx) != 0, "reject overspend append");

  chain.blocks[1].hash[0] = 'f';
  ASSERT(chippy_chain_validate(&chain) != 0, "reject tampered block hash");

  chippy_chain_free(&chain);
}

static void test_storage_roundtrip(void)
{
  chippy_chain chain;
  chippy_tx tx;
  chippy_block saved;
  unsigned char mint_sk[crypto_sign_SECRETKEYBYTES];
  unsigned char pk[crypto_sign_PUBLICKEYBYTES];
  unsigned char sk[crypto_sign_SECRETKEYBYTES];
  char addr[CHIPPY_HEX_ADDR_LEN + 1];
  uint64_t balance;
  char *dir;

  dir = make_temp_dir();
  if (dir == NULL) {
    fail("temp dir storage");
    return;
  }

  chippy_chain_init(&chain);
  ASSERT(chippy_storage_init(dir) == 0, "storage init roundtrip dir");
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "load genesis for roundtrip");
  ASSERT(chippy_storage_mint_load_sec(dir, mint_sk) == 0, "mint key for roundtrip");
  ASSERT(chippy_keypair_generate(pk, sk) == 0, "recipient keypair");
  ASSERT(chippy_pubkey_to_address(pk, addr) == 0, "recipient address");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 500;
  ASSERT(sign_tx(&tx, mint_sk) == 0, "sign mint for disk");
  ASSERT(append_block(&chain, &tx) == 0, "append mint in memory");
  saved = chain.blocks[chain.block_count - 1];
  ASSERT(chippy_storage_append_block(dir, &saved) == 0, "write block to chain file");
  chippy_chain_free(&chain);

  chippy_chain_init(&chain);
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "reload chain from disk");
  ASSERT(chippy_chain_validate(&chain) == 0, "reloaded chain validates");
  ASSERT(chippy_chain_balance(&chain, addr, &balance) == 0, "balance from disk chain");
  ASSERT(balance == 500, "mint amount on disk");
  chippy_chain_free(&chain);
  free(dir);
}

int main(void)
{
  char *dir;

  failures = 0;
  if (sodium_init() < 0) {
    fprintf(stderr, "sodium_init failed\n");
    return 1;
  }

  test_hex();
  test_crypto();
  test_tx_format_parse();

  dir = make_temp_dir();
  if (dir == NULL) {
    fail("temp dir");
    fprintf(stderr, "%d test(s) failed\n", failures);
    return failures > 0 ? 1 : 0;
  }
  test_dir_lock(dir);
  test_chain_flow(dir);
  free(dir);
  test_storage_roundtrip();

  if (failures > 0) {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }
  fprintf(stdout, "all tests passed\n");
  return 0;
}
