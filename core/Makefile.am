# include paths: local headers plus libsodium from configure
AM_CPPFLAGS = -I$(srcdir)/include @LIBSODIUM_CFLAGS@

# engine + high-level ops (no CLI)
CHIPPY_LIB_SRC = \
	src/util.c \
	src/crypto.c \
	src/tx.c \
	src/chain.c \
	src/lock.c \
	src/storage.c \
	src/ops.c

noinst_LIBRARIES = libchippy.a
libchippy_a_SOURCES = \
	include/chippy.h \
	include/chippy_ops.h \
	$(CHIPPY_LIB_SRC)

bin_PROGRAMS = chippy
chippy_SOURCES = src/main.c
chippy_LDADD = libchippy.a @LIBSODIUM_LIBS@

check_PROGRAMS = test_core
test_core_SOURCES = tests/test_core.c
test_core_LDADD = libchippy.a @LIBSODIUM_LIBS@

TESTS = test_core tests/test_cli.sh
EXTRA_DIST = tests/test_cli.sh
