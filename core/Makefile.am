AM_CPPFLAGS = \
	-I$(srcdir)/include \
	-I$(srcdir)/vendor \
	-DCHIPPY_STATIC_DIR=\"$(abs_srcdir)/static\" \
	@LIBSODIUM_CFLAGS@

CHIPPY_LIB_SRC = \
	src/util.c \
	src/mint_keys.c \
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
chippy_SOURCES = \
	src/main.c \
	src/cmd_local.c \
	src/cmd_serve.c \
	src/http_server.c \
	src/http_api.c \
	src/json_util.c \
	vendor/picohttpparser.c

chippy_LDADD = libchippy.a @LIBSODIUM_LIBS@

check_PROGRAMS = test_core
test_core_SOURCES = tests/test_core.c
test_core_LDADD = libchippy.a @LIBSODIUM_LIBS@

TESTS = test_core tests/test_cli.sh tests/test_serve.sh
EXTRA_DIST = \
	tests/test_cli.sh \
	tests/test_serve.sh \
	static/index.html \
	static/style.css \
	static/app.js \
	vendor/picohttpparser.h
