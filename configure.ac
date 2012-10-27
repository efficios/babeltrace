#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([babeltrace],[1.0.0],[mathieu dot desnoyers at efficios dot com])
AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])

AM_PROG_MKDIR_P

AC_CONFIG_HEADERS([config.h])

AC_SYS_LARGEFILE

# Checks for programs.
AC_PROG_CC
AC_PROG_MAKE_SET
LT_INIT
AC_PROG_YACC
AC_PROG_LEX

AC_DEFUN([AC_PROG_BISON], [AC_CHECK_PROGS(BISON, bison, bison)]) 

AM_PATH_GLIB_2_0(2.22.0, ,AC_MSG_ERROR([glib is required in order to compile BabelTrace - download it from ftp://ftp.gtk.org/pub/gtk]) , gmodule)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero gettimeofday munmap strtoul])

# Check for libuuid
AC_CHECK_LIB([uuid], [uuid_generate],
[
	AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_LIBUUID], 1, [Has libuuid support.])
	have_libuuid=yes
],
[
	# libuuid not found, check for uuid_create in libc.
	AC_CHECK_LIB([c], [uuid_create],
	[
		AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_LIBC_UUID], 1, [Has libc uuid support.])
		have_libc_uuid=yes
	],
	[
		AC_MSG_ERROR([Cannot find libuuid uuid_generate nor libc uuid_create. Use [LDFLAGS]=-Ldir to specify their location.])
	])
]
)
AM_CONDITIONAL([BABELTRACE_BUILD_WITH_LIBUUID], [test "x$have_libuuid" = "xyes"])
AM_CONDITIONAL([BABELTRACE_BUILD_WITH_LIBC_UUID], [test "x$have_libc_uuid" = "xyes"])

# Check for fmemopen
AC_CHECK_LIB([c], [fmemopen],
[
	AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_FMEMOPEN], 1, [Has fmemopen support.])
]
)

# Check for open_memstream
AC_CHECK_LIB([c], [open_memstream],
[
	AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_OPEN_MEMSTREAM], 1, [Has open_memstream support.])
]
)

AC_CHECK_LIB([popt], [poptGetContext], [],
        [AC_MSG_ERROR([Cannot find popt.])]
)

pkg_modules="gmodule-2.0 >= 2.0.0"
PKG_CHECK_MODULES(GMODULE, [$pkg_modules])
AC_SUBST(PACKAGE_LIBS)

LIBS="$LIBS $GMODULE_LIBS"
PACKAGE_CFLAGS="$GMODULE_CFLAGS -Wall -Wformat -include config.h"
AC_SUBST(PACKAGE_CFLAGS)

babeltraceincludedir="${includedir}/babeltrace"
AC_SUBST(babeltraceincludedir)

babeltracectfincludedir="${includedir}/babeltrace/ctf"
AC_SUBST(babeltracectfincludedir)

AC_CONFIG_FILES([
	Makefile
	types/Makefile
	formats/Makefile
	formats/ctf/Makefile
	formats/ctf/types/Makefile
	formats/ctf-text/Makefile
	formats/ctf-text/types/Makefile
	formats/bt-dummy/Makefile
	formats/ctf/metadata/Makefile
	converter/Makefile
	doc/Makefile
	lib/Makefile
	lib/prio_heap/Makefile
	include/Makefile
	tests/Makefile
	extras/Makefile
	extras/valgrind/Makefile
])
AC_OUTPUT
