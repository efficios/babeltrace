#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_INIT([babeltrace],[1.2.4],[mathieu dot desnoyers at efficios dot com])

# Following the numbering scheme proposed by libtool for the library version
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST([BABELTRACE_LIBRARY_VERSION], [1:0:0])

AC_CONFIG_AUX_DIR([config])
AC_CANONICAL_TARGET
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_MKDIR_P

AC_CONFIG_HEADERS([config.h])

AC_SYS_LARGEFILE

# Checks for programs.
AC_PROG_CC_STDC
AC_PROG_MAKE_SET
LT_INIT
AC_PROG_YACC
AC_PROG_LEX

if test ! -f "$srcdir/formats/ctf/metadata/ctf-parser.h"; then
        if test x"$YACC" != "xbison -y"; then
                AC_MSG_ERROR([[bison not found and is required when building from git.
                Please install bison]])
        fi
fi

if test ! -f "$srcdir/formats/ctf/metadata/ctf-lexer.c"; then
        if test x"$LEX" != "xflex"; then
                AC_MSG_ERROR([[flex not found and is required when building from git.
                Please install flex]])
        fi
fi


AM_PATH_GLIB_2_0(2.22.0, ,AC_MSG_ERROR([glib is required in order to compile BabelTrace - download it from ftp://ftp.gtk.org/pub/gtk]) , gmodule)

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MMAP
AC_CHECK_FUNCS([bzero gettimeofday munmap strtoul])

# Check for MinGW32.
MINGW32=no
case $host in
  *-*-mingw*)
	MINGW32=yes;;
esac

AM_CONDITIONAL([BABELTRACE_BUILD_WITH_MINGW], [test "x$MINGW32" = "xyes"])

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
		# for MinGW32 we have our own internal implemenation of uuid using Windows functions.
		if test "x$MINGW32" = xno; then
			AC_MSG_ERROR([Cannot find libuuid uuid_generate nor libc uuid_create. Use [LDFLAGS]=-Ldir to specify their location.])
		fi
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


# For Python
# SWIG version needed or newer:
swig_version=2.0.0

AC_ARG_ENABLE([python-bindings],
              [AC_HELP_STRING([--enable-python-bindings],
                              [generate Python bindings])],
              [enable_python=yes], [enable_python=no])

AM_CONDITIONAL([USE_PYTHON], [test "x${enable_python:-yes}" = xyes])

if test "x${enable_python:-yes}" = xyes; then
  AX_PKG_SWIG($swig_version, [], [ AC_MSG_ERROR([SWIG $swig_version or newer is needed]) ])
  AM_PATH_PYTHON

  AC_ARG_VAR([PYTHON_INCLUDE], [Include flags for python, bypassing python-config])
  AC_ARG_VAR([PYTHON_CONFIG], [Path to python-config])
  AS_IF([test -z "$PYTHON_INCLUDE"], [
    AS_IF([test -z "$PYTHON_CONFIG"], [
      AC_PATH_PROGS([PYTHON_CONFIG],
                    [python$PYTHON_VERSION-config python-config],
                    [no],
                    [`dirname $PYTHON`])
      AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON. Do you have python-dev installed?])])
    ])
    AC_MSG_CHECKING([python include flags])
    PYTHON_INCLUDE=`$PYTHON_CONFIG --includes`
    AC_MSG_RESULT([$PYTHON_INCLUDE])
  ])

else
    AC_MSG_NOTICE([You may configure with --enable-python-bindings ]dnl
[if you want Python bindings.])

fi

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

babeltracectfwriterincludedir="${includedir}/babeltrace/ctf-writer"
AC_SUBST(babeltracectfwriterincludedir)

AC_CONFIG_FILES([
	Makefile
	types/Makefile
	compat/Makefile
	formats/Makefile
	formats/ctf/Makefile
	formats/ctf/types/Makefile
	formats/ctf-text/Makefile
	formats/ctf-text/types/Makefile
	formats/ctf-metadata/Makefile
	formats/bt-dummy/Makefile
	formats/lttng-live/Makefile
	formats/ctf/metadata/Makefile
	formats/ctf/writer/Makefile
	converter/Makefile
	doc/Makefile
	lib/Makefile
	lib/prio_heap/Makefile
	include/Makefile
	bindings/Makefile
	bindings/python/Makefile
	tests/Makefile
	tests/bin/Makefile
	tests/lib/Makefile
	tests/utils/Makefile
	tests/utils/tap/Makefile
	extras/Makefile
	extras/valgrind/Makefile
	babeltrace.pc
	babeltrace-ctf.pc
])
AC_OUTPUT
