#ifndef CHIPPY_CMD_LOCAL_H
#define CHIPPY_CMD_LOCAL_H

/* apply --dir from argv before running a data-dir command */
void chippy_apply_dir_flag(int argc, char **argv);

int cmd_init(int argc, char **argv);
int cmd_keygen(int argc, char **argv);
int cmd_sign_mint(int argc, char **argv);
int cmd_sign_transfer(int argc, char **argv);
int cmd_mint(int argc, char **argv);
int cmd_transfer(int argc, char **argv);
int cmd_balance(int argc, char **argv);
int cmd_mint_key_add(int argc, char **argv);
int cmd_validate(int argc, char **argv);

#endif /* CHIPPY_CMD_LOCAL_H */
