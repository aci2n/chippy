#include "chippy.h"
#include "chippy_ops.h"

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

static int authorize_mint_key(const char *dir, char *mint_addr_out, char *mint_sec_out)
{
  if (chippy_op_keygen(mint_addr_out, mint_sec_out) != 0) {
    return -1;
  }
  return chippy_storage_add_authorized_mint_key(dir, mint_addr_out);
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
  char mint_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char mint_sec_hex[CHIPPY_HEX_SEC_LEN + 1];
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
  ASSERT(authorize_mint_key(dir, mint_addr, mint_sec_hex) == 0, "authorize mint");
  ASSERT(chippy_hex_decode(mint_sec_hex, mint_sk, sizeof(mint_sk)) ==
             (int)crypto_sign_SECRETKEYBYTES,
         "mint secret decode");
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "load genesis");
  ASSERT(chippy_chain_validate(&chain) == 0, "genesis validates");

  ASSERT(chippy_keypair_generate(alice_pk, alice_sk) == 0, "alice keypair");
  ASSERT(chippy_pubkey_to_address(alice_pk, alice_addr) == 0, "alice address");
  ASSERT(chippy_keypair_generate(bob_pk, bob_sk) == 0, "bob keypair");
  ASSERT(chippy_pubkey_to_address(bob_pk, bob_addr) == 0, "bob address");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, alice_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 1000;
  ASSERT(chippy_tx_sign(&tx, mint_sk) == 0, "sign mint tx");
  ASSERT(append_block(&chain, &tx) == 0, "append mint block");
  ASSERT(chippy_chain_balance(&chain, alice_addr, &balance) == 0, "alice balance after mint");
  ASSERT(balance == 1000, "alice has 1000");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, alice_addr, CHIPPY_HEX_ADDR_LEN);
  strncpy(tx.to, bob_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 250;
  ASSERT(chippy_tx_sign(&tx, alice_sk) == 0, "sign transfer tx");
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
  ASSERT(chippy_tx_sign(&tx, alice_sk) == 0, "sign overspend tx");
  ASSERT(append_block(&chain, &tx) != 0, "reject overspend append");

  chain.blocks[1].hash[0] ^= (char)1;
  ASSERT(chippy_chain_validate(&chain) != 0, "reject tampered block hash");

  chippy_chain_free(&chain);
}

static void test_ops_flow(char *dir)
{
  chippy_tx tx;
  char mint_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char mint_sec[CHIPPY_HEX_SEC_LEN + 1];
  char alice_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char alice_sec[CHIPPY_HEX_SEC_LEN + 1];
  char bob_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char bob_sec[CHIPPY_HEX_SEC_LEN + 1];
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  uint64_t balance;

  ASSERT(chippy_op_init(dir) == 0, "op init");
  ASSERT(authorize_mint_key(dir, mint_addr, mint_sec) == 0, "authorize mint");
  ASSERT(chippy_op_keygen(alice_addr, alice_sec) == 0, "op keygen alice");
  ASSERT(chippy_op_keygen(bob_addr, bob_sec) == 0, "op keygen bob");
  (void)bob_sec;
  ASSERT(chippy_op_sign_mint(mint_sec, mint_addr, alice_addr, 1000, sig) == 0, "op sign mint");
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  tx.from[0] = '\0';
  strncpy(tx.to, alice_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 1000;
  strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_op_submit_tx(dir, &tx) == 0, "op submit mint");
  ASSERT(chippy_op_balance(dir, alice_addr, &balance) == 0, "op balance alice");
  ASSERT(balance == 1000, "op alice has 1000");
  ASSERT(chippy_op_sign_transfer(alice_sec, alice_addr, bob_addr, 250, sig) == 0,
         "op sign transfer");
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, alice_addr, CHIPPY_HEX_ADDR_LEN);
  strncpy(tx.to, bob_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 250;
  strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_op_transfer(dir, &tx) == 0, "op transfer");
  ASSERT(chippy_op_balance(dir, alice_addr, &balance) == 0, "op balance alice after");
  ASSERT(balance == 750, "op alice has 750");
  ASSERT(chippy_op_balance(dir, bob_addr, &balance) == 0, "op balance bob");
  ASSERT(balance == 250, "op bob has 250");
  ASSERT(chippy_op_validate(dir) == 0, "op validate");
}

static void test_ops_overspend_rejected(char *dir)
{
  chippy_chain chain;
  chippy_tx tx;
  char mint_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char mint_sec[CHIPPY_HEX_SEC_LEN + 1];
  char alice_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char alice_sec[CHIPPY_HEX_SEC_LEN + 1];
  char bob_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char bob_sec[CHIPPY_HEX_SEC_LEN + 1];
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  uint64_t balance;

  ASSERT(chippy_op_init(dir) == 0, "overspend init");
  ASSERT(authorize_mint_key(dir, mint_addr, mint_sec) == 0, "overspend authorize mint");
  ASSERT(chippy_op_keygen(alice_addr, alice_sec) == 0, "overspend alice");
  ASSERT(chippy_op_keygen(bob_addr, bob_sec) == 0, "overspend bob");
  (void)bob_sec;

  ASSERT(chippy_op_sign_mint(mint_sec, mint_addr, alice_addr, 1000, sig) == 0, "overspend sign mint");
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  tx.from[0] = '\0';
  strncpy(tx.to, alice_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 1000;
  strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_op_submit_tx(dir, &tx) == 0, "overspend submit mint");

  ASSERT(chippy_op_sign_transfer(alice_sec, alice_addr, bob_addr, 100, sig) == 0,
         "overspend sign small transfer");
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, alice_addr, CHIPPY_HEX_ADDR_LEN);
  strncpy(tx.to, bob_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 100;
  strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_op_submit_tx(dir, &tx) == 0, "overspend submit small transfer");

  ASSERT(chippy_op_sign_transfer(alice_sec, alice_addr, bob_addr, 1000, sig) == 0,
         "overspend sign large transfer");
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_TRANSFER;
  strncpy(tx.from, alice_addr, CHIPPY_HEX_ADDR_LEN);
  strncpy(tx.to, bob_addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 1000;
  strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_op_submit_tx(dir, &tx) != 0, "overspend reject large transfer");

  ASSERT(chippy_op_balance(dir, alice_addr, &balance) == 0, "overspend balance alice");
  ASSERT(balance == 900, "overspend alice still has 900");
  ASSERT(chippy_op_balance(dir, bob_addr, &balance) == 0, "overspend balance bob");
  ASSERT(balance == 100, "overspend bob still has 100");
  ASSERT(chippy_op_validate(dir) == 0, "overspend chain still valid");

  chippy_chain_init(&chain);
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "overspend reload chain");
  ASSERT(chain.block_count == 3, "overspend chain has genesis+mint+transfer only");
  chippy_chain_free(&chain);
}

static void test_authorized_mint_keys(char *dir)
{
  chippy_chain chain;
  chippy_tx tx;
  char mint_a[CHIPPY_HEX_ADDR_LEN + 1];
  char mint_a_sec[CHIPPY_HEX_SEC_LEN + 1];
  char mint_b[CHIPPY_HEX_ADDR_LEN + 1];
  char mint_b_sec[CHIPPY_HEX_SEC_LEN + 1];
  char alice[CHIPPY_HEX_ADDR_LEN + 1];
  char alice_sec[CHIPPY_HEX_SEC_LEN + 1];
  char sig[CHIPPY_HEX_SIG_LEN + 1];
  uint64_t balance;

  ASSERT(chippy_op_init(dir) == 0, "init for mint keys test");
  ASSERT(authorize_mint_key(dir, mint_a, mint_a_sec) == 0, "authorize mint a");
  ASSERT(chippy_op_keygen(mint_b, mint_b_sec) == 0, "second mint key");
  ASSERT(chippy_storage_add_authorized_mint_key(dir, mint_b) == 0, "authorize mint b");
  chippy_chain_init(&chain);
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "load chain with mint addrs");
  ASSERT(chain.mint_keys.count == 2, "two authorized mint addrs");
  ASSERT(chippy_mint_keys_contains(&chain.mint_keys, mint_a) != 0, "mint a authorized");
  ASSERT(chippy_mint_keys_contains(&chain.mint_keys, mint_b) != 0, "mint b authorized");
  chippy_chain_free(&chain);
  ASSERT(chippy_op_keygen(alice, alice_sec) == 0, "alice recipient");
  (void)alice_sec;
  ASSERT(chippy_op_sign_mint(mint_a_sec, mint_b, alice, 50, sig) != 0,
         "reject mint secret/address mismatch");
  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  tx.from[0] = '\0';
  strncpy(tx.to, alice, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 50;
  ASSERT(chippy_op_sign_mint(mint_b_sec, mint_b, alice, 50, sig) == 0, "sign mint as b");
  strncpy(tx.sig, sig, CHIPPY_HEX_SIG_LEN);
  ASSERT(chippy_op_submit_tx(dir, &tx) == 0, "submit mint from key b");
  ASSERT(chippy_op_balance(dir, alice, &balance) == 0, "alice balance");
  ASSERT(balance == 50, "mint from second key");
}

static void test_storage_roundtrip(void)
{
  chippy_chain chain;
  chippy_tx tx;
  chippy_block saved;
  char mint_addr[CHIPPY_HEX_ADDR_LEN + 1];
  char mint_sec_hex[CHIPPY_HEX_SEC_LEN + 1];
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
  ASSERT(authorize_mint_key(dir, mint_addr, mint_sec_hex) == 0, "authorize mint");
  ASSERT(chippy_hex_decode(mint_sec_hex, mint_sk, sizeof(mint_sk)) ==
             (int)crypto_sign_SECRETKEYBYTES,
         "mint secret roundtrip");
  ASSERT(chippy_storage_load_chain(dir, &chain) == 0, "load genesis for roundtrip");
  ASSERT(chippy_keypair_generate(pk, sk) == 0, "recipient keypair");
  ASSERT(chippy_pubkey_to_address(pk, addr) == 0, "recipient address");

  memset(&tx, 0, sizeof(tx));
  tx.type = CHIPPY_TX_MINT;
  strncpy(tx.to, addr, CHIPPY_HEX_ADDR_LEN);
  tx.amount = 500;
  ASSERT(chippy_tx_sign(&tx, mint_sk) == 0, "sign mint for disk");
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

  dir = make_temp_dir();
  if (dir == NULL) {
    fail("temp dir ops");
  } else {
    test_ops_flow(dir);
    test_ops_overspend_rejected(dir);
    free(dir);
  }

  dir = make_temp_dir();
  if (dir == NULL) {
    fail("temp dir authorized mint keys");
  } else {
    test_authorized_mint_keys(dir);
    free(dir);
  }

  test_storage_roundtrip();

  if (failures > 0) {
    fprintf(stderr, "%d test(s) failed\n", failures);
    return 1;
  }
  fprintf(stdout, "all tests passed\n");
  return 0;
}
