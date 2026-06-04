/*
 * High-level Chippy operations (lock, load chain, mutate, persist).
 * For CLI, HTTP server, and tests. Requires sodium_init() once per process.
 */
#ifndef CHIPPY_OPS_H
#define CHIPPY_OPS_H

#include "chippy.h"

#include <stdint.h>

/* create .chippy data dir (mint keys, genesis) */
int chippy_op_init(const char *dir);

/* append signed mint tx (server signs with mint.sec) */
int chippy_op_mint(const char *dir, const char *to_addr, uint64_t amount);

/*
 * append pre-signed transfer; tx must have from, to, amount, sig set.
 * verifies signature before append.
 */
int chippy_op_transfer(const char *dir, const chippy_tx *tx);

/* replay chain for addr balance */
int chippy_op_balance(const char *dir, const char *addr_hex, uint64_t *balance_out);

/* full chain validation */
int chippy_op_validate(const char *dir);

/* client-side helpers (no data dir; no lock) */

/* write address and secret hex strings into caller buffers */
int chippy_op_keygen(char *addr_out, char *secret_hex_out);

/*
 * sign transfer; secret must match from_addr.
 * writes sig hex into sig_out (CHIPPY_HEX_SIG_LEN + 1).
 */
int chippy_op_sign_transfer(const char *secret_hex, const char *from_addr, const char *to_addr,
                            uint64_t amount, char *sig_out);

#endif /* CHIPPY_OPS_H */
