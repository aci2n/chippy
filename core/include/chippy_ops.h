/*
 * High-level Chippy operations (lock, load chain, mutate, persist).
 * For CLI, HTTP server, and tests. Requires sodium_init() once per process.
 */
#ifndef CHIPPY_OPS_H
#define CHIPPY_OPS_H

#include "chippy.h"

#include <stdint.h>

/* create .chippy data dir (empty config + genesis block) */
int chippy_op_init(const char *dir);

/*
 * append a signed tx (mint or transfer). verifies signature then persists.
 * mint: type MINT, to, amount, sig (from empty). transfer: from, to, amount, sig.
 */
int chippy_op_submit_tx(const char *dir, const chippy_tx *tx);

/* append pre-signed mint */
int chippy_op_mint(const char *dir, const chippy_tx *tx);

/* append pre-signed transfer */
int chippy_op_transfer(const char *dir, const chippy_tx *tx);

/* replay chain for addr balance */
int chippy_op_balance(const char *dir, const char *addr_hex, uint64_t *balance_out);

/* full chain validation */
int chippy_op_validate(const char *dir);

/* client-side helpers (no data dir; no lock) */

int chippy_op_keygen(char *addr_out, char *secret_hex_out);

int chippy_op_sign_transfer(const char *secret_hex, const char *from_addr, const char *to_addr,
                            uint64_t amount, char *sig_out);

/*
 * sign mint; mint_secret must match mint_address (mint authority pubkey hex).
 */
int chippy_op_sign_mint(const char *mint_secret_hex, const char *mint_address,
                        const char *to_addr, uint64_t amount, char *sig_out);

#endif /* CHIPPY_OPS_H */
