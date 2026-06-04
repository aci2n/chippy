AM_CPPFLAGS = \
	-I$(srcdir)/include \
	-I$(srcdir)/vendor \
	-DCHIPPY_STATIC_DIR=\"$(abs_srcdir)/static\" \
	@LIBSODIUM_CFLAGS@

CHIPPY_LIB_SRC = \
	src/log.c \
	src/util.c \
	src/mint_keys.c \
	src/crypto.c \
	src/tx.c \
	src/chain.c \
	src/lock.c \
	src/storage.c \
	src/ops.c

bin_PROGRAMS = chippy
chippy_SOURCES = \
	src/main.c \
	src/cmd_local.c \
	src/cmd_serve.c \
	src/http_server.c \
	src/http_api.c \
	src/json_util.c \
	vendor/picohttpparser.c \
	include/chippy.h \
	include/chippy_ops.h \
	include/log.h \
	$(CHIPPY_LIB_SRC)

chippy_LDADD = @LIBSODIUM_LIBS@

check_PROGRAMS = test_core
test_core_SOURCES = \
	tests/test_core.c \
	include/chippy.h \
	include/chippy_ops.h \
	include/log.h \
	$(CHIPPY_LIB_SRC)

test_core_LDADD = @LIBSODIUM_LIBS@

TESTS = test_core tests/test_cli.sh tests/test_serve.sh
EXTRA_DIST = \
	tests/test_cli.sh \
	tests/test_serve.sh \
	static/index.html \
	static/style.css \
	static/app.js \
	static/crypto.js \
	vendor/picohttpparser.h
