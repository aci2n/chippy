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
# debug logging (LOG_DEBUG); on by default
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--disable-debug], [disable debug logging (default: enabled)])],
  [chippy_debug=$enableval],
  [chippy_debug=yes])
AS_IF([test "x$chippy_debug" != "xno"],
  [AC_DEFINE([CHIPPY_DEBUG], [1], [Enable debug logging])])
# AddressSanitizer + UndefinedBehaviorSanitizer (debug builds)
AC_ARG_ENABLE([sanitize],
  [AS_HELP_STRING([--enable-sanitize],
    [build with -fsanitize=address,undefined (for make check)])],
  [chippy_sanitize=$enableval],
  [chippy_sanitize=no])
AM_CONDITIONAL([CHIPPY_SANITIZE], [test "x$chippy_sanitize" = "xyes"])
AS_IF([test "x$chippy_sanitize" = "xyes"],
  [SAN_CFLAGS="-g -O1 -fsanitize=address,undefined -fno-omit-frame-pointer"
   SAN_LDFLAGS="-fsanitize=address,undefined"])
AC_SUBST([SAN_CFLAGS])
AC_SUBST([SAN_LDFLAGS])
# generate Makefiles from Makefile.in templates
AC_CONFIG_FILES([Makefile core/Makefile])
# emit configured files and finish
AC_OUTPUT
