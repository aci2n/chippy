# include paths: local headers plus libsodium from configure
AM_CPPFLAGS = -I$(srcdir)/include @LIBSODIUM_CFLAGS@
# installable program built in this directory
bin_PROGRAMS = chippy
# public API header
# util: hex encode/decode
# crypto: ed25519 and sha256
# tx: transaction sign/verify and line format
# chain: blocks, validation, balances
# storage: .chippy/ load/save
# cli: command-line tool
chippy_SOURCES = \
	include/chippy.h \
	src/util.c \
	src/crypto.c \
	src/tx.c \
	src/chain.c \
	src/storage.c \
	cli/main.c
# link against libsodium
chippy_LDADD = @LIBSODIUM_LIBS@
