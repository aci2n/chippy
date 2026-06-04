AM_CPPFLAGS = -I$(srcdir)/include @LIBSODIUM_CFLAGS@
bin_PROGRAMS = chippy
chippy_SOURCES = \
	include/chippy.h \
	src/util.c \
	src/crypto.c \
	src/tx.c \
	src/chain.c \
	src/storage.c \
	cli/main.c
chippy_LDADD = @LIBSODIUM_LIBS@
