# include paths: local headers plus libsodium from configure
AM_CPPFLAGS = -I$(srcdir)/include @LIBSODIUM_CFLAGS@
# engine sources used by chippy and unit tests (not the cli)
CHIPPY_ENGINE_SRC = \
	src/util.c \
	src/crypto.c \
	src/tx.c \
	src/chain.c \
	src/lock.c \
	src/storage.c
# installable program built in this directory
bin_PROGRAMS = chippy
# public API header
# util: hex encode/decode
# crypto: ed25519 and sha256
# tx: transaction sign/verify and line format
# chain: blocks, validation, balances
# storage: .chippy/ load/save
# main: command-line entry point
chippy_SOURCES = \
	include/chippy.h \
	$(CHIPPY_ENGINE_SRC) \
	src/main.c
# link against libsodium
chippy_LDADD = @LIBSODIUM_LIBS@
# unit test binary (not installed)
check_PROGRAMS = test_core
test_core_SOURCES = tests/test_core.c $(CHIPPY_ENGINE_SRC)
test_core_LDADD = @LIBSODIUM_LIBS@
# scripts and binaries run by make check
TESTS = test_core tests/test_cli.sh
# ship test script in source tarball
EXTRA_DIST = tests/test_cli.sh
