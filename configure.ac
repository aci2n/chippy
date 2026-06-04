# require autoconf 2.69 or newer
AC_PREREQ([2.69])
# package name, version, bug-report address (empty)
AC_INIT([chippy], [0.1.0], [])
# automake: non-gnu layout; compile objects in subdir trees
AM_INIT_AUTOMAKE([foreign subdir-objects])
# locate a C compiler
AC_PROG_CC
AC_PROG_RANLIB
# libsodium via pkg-config; sets LIBSODIUM_CFLAGS and LIBSODIUM_LIBS
PKG_CHECK_MODULES([LIBSODIUM], [libsodium >= 1.0.0])
# generate Makefiles from Makefile.in templates
AC_CONFIG_FILES([Makefile core/Makefile web/Makefile])
# emit configured files and finish
AC_OUTPUT
