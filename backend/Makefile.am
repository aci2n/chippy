AM_CPPFLAGS = \
	-I$(srcdir)/include \
	-I$(top_srcdir)/core/include \
	-I$(srcdir)/vendor

bin_PROGRAMS = backend
backend_SOURCES = \
	src/main.c \
	src/server.c \
	src/api.c \
	src/json_util.c \
	vendor/picohttpparser.c

backend_LDADD = $(top_builddir)/core/libchippy.a @LIBSODIUM_LIBS@

$(top_builddir)/core/libchippy.a:
	$(MAKE) -C $(top_builddir)/core libchippy.a

backend_DEPENDENCIES = $(top_builddir)/core/libchippy.a

check-local:
	@CHIPPY_BACKEND="$(top_builddir)/backend/backend" \
	 CHIPPY="$(top_builddir)/core/chippy" \
	 $(srcdir)/tests/test_api.sh

EXTRA_DIST = tests/test_api.sh vendor/picohttpparser.h
