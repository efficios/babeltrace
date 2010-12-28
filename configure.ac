#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([babeltrace], [0.1], [mathieu dot desnoyers at efficios dot com])
AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])

AM_PROG_MKDIR_P

AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AM_PATH_GLIB_2_0(2.4.0, ,AC_MSG_ERROR([glib is required in order to compile BabelTrace - download it from ftp://ftp.gtk.org/pub/gtk]) , gmodule)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero gettimeofday munmap strtoul])

pkg_modules="gmodule-2.0 >= 2.0.0"
PKG_CHECK_MODULES(GMODULE, [$pkg_modules])
AC_SUBST(PACKAGE_LIBS)

PACKAGE_CFLAGS="$GMODULE_CFLAGS"
AC_SUBST(PACKAGE_CFLAGS)

AC_CONFIG_FILES([
	Makefile
	types/Makefile
	formats/Makefile
	formats/ctf/Makefile
	formats/ctf/types/Makefile
	tests/Makefile
])
AC_OUTPUT
