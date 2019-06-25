AC_PREREQ([2.50])
AC_INIT([babeltrace],[1.5.7],[jeremie.galarneau@efficios.com],[],[https://diamon.org/babeltrace])
AC_SUBST([BABELTRACE_LIBRARY_VERSION], [1:0:0])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip tar-ustar])
AM_MAINTAINER_MODE([enable])

# Enable silent rules if available (Introduced in AM 1.11)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_REQUIRE_AUX_FILE([tap-driver.sh])

# Checks for C compiler
AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE
AC_PROG_CC
AC_PROG_CC_STDC

# Checks for programs.
AC_PROG_MAKE_SET
LT_INIT
AC_PROG_YACC
AC_PROG_LEX
AC_PROG_MKDIR_P
AC_PROG_LN_S

AX_C___ATTRIBUTE__
AS_IF([test "x$ax_cv___attribute__" = "xyes"],
	[:],
	[AC_MSG_ERROR([The compiler does not support __attribute__ extensions])])

AX_PTHREAD(,[AC_MSG_ERROR([Could not configure pthreads support])])
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"
CC="$PTHREAD_CC"

# Check linker option
AX_APPEND_LINK_FLAGS([-Wl,--no-as-needed], [LD_NO_AS_NEEDED])
AC_SUBST([LD_NO_AS_NEEDED])

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([ \
	fcntl.h \
	float.h \
	libintl.h \
	limits.h \
	malloc.h \
	netdb.h \
	netinet/in.h \
	stddef.h \
	sys/socket.h \
])

if test ! -f "$srcdir/formats/ctf/metadata/ctf-parser.h"; then
        if test x"$(basename "$YACC")" != "xbison -y"; then
                AC_MSG_ERROR([[bison not found and is required when building from git.
                Please install bison]])
        fi
        AC_PATH_PROG([BISON],[bison])
        AX_PROG_BISON_VERSION([2.4], [],[
                AC_MSG_ERROR([[Bison >= 2.4 is required when building from git]])
        ])
fi

if test ! -f "$srcdir/formats/ctf/metadata/ctf-lexer.c"; then
        if test x"$LEX" != "xflex"; then
                AC_MSG_ERROR([[flex not found and is required when building from git.
                Please install flex]])
        fi
        AC_PATH_PROG([FLEX],[flex])
        AX_PROG_FLEX_VERSION([2.5.35], [],[
                AC_MSG_ERROR([[Flex >= 2.5.35 is required when building from git]])
        ])
fi

AM_PATH_GLIB_2_0(2.22.0, ,AC_MSG_ERROR([glib is required in order to compile BabelTrace - download it from ftp://ftp.gtk.org/pub/gtk]) , gmodule)

# Check what libraries are required on this platform to link sockets programs.
AX_LIB_SOCKET_NSL

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_REALLOC
AC_CHECK_FUNCS([ \
	atexit \
	dirfd \
	dup2 \
	ftruncate \
	gethostbyname \
	gethostname \
	gettimeofday \
	localtime_r \
	memchr \
	memset \
	mkdir \
	mkdtemp \
	munmap \
	rmdir \
	setenv \
	socket \
	strchr \
	strdup \
	strerror \
	strndup \
	strnlen \
	strrchr \
	strtoul \
	strtoull \
	tzset \
	uname \
])

MINGW32=no
DEFAULT_ENABLE_DEBUG_INFO=yes
LT_NO_UNDEFINED=""
AS_CASE([$host_os],
  [solaris*|darwin*],
    [
	DEFAULT_ENABLE_DEBUG_INFO=no
    ],
  [mingw*],
    [
	MINGW32=yes
	LT_NO_UNDEFINED="-no-undefined"
    ],
  [cygwin*],
    [
	LT_NO_UNDEFINED="-no-undefined"
	DEFAULT_ENABLE_DEBUG_INFO=no
    ]
)

AM_CONDITIONAL([BABELTRACE_BUILD_WITH_MINGW], [test "x$MINGW32" = "xyes"])
AC_SUBST(LT_NO_UNDEFINED)

# Check what libraries are required on this platform to link sockets programs.
AX_LIB_SOCKET_NSL

# Check for uuid in system libs
AC_CHECK_FUNCS([uuid_generate],
[
  AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_LIBUUID], 1, [Has libuuid support.])
  link_with_libuuid=no
],
[
  # First, check the pkg-config module is available, otherwise explicitly check
  # for libuuid, or uuid support in the C-library.
  PKG_CHECK_MODULES([UUID], [uuid],
  [
    LIBS="${UUID_LIBS} ${LIBS}"
    CFLAGS="${CFLAGS} ${UUID_CFLAGS}"
    AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_LIBUUID], 1, [Has libuuid support.])
    link_with_libuuid=yes
  ],
  [
    # Check for libuuid
    AC_CHECK_LIB([uuid], [uuid_generate],
    [
      AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_LIBUUID], 1, [Has libuuid support.])
      link_with_libuuid=yes
    ],
    [
      # libuuid not found, check for uuid_create in libc.
      AC_CHECK_LIB([c], [uuid_create],
      [
        AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_LIBC_UUID], 1, [Has libc uuid support.])
        link_with_libc_uuid=yes
      ],
      [
        # for MinGW32 we have our own internal implementation of uuid using Windows functions.
        if test "x$MINGW32" = xno; then
          AC_MSG_ERROR([Cannot find libuuid uuid_generate nor libc uuid_create. Use [LDFLAGS]=-Ldir to specify their location.])
        fi
      ])
    ])
  ])
])

AM_CONDITIONAL([BABELTRACE_BUILD_WITH_LIBUUID], [test "x$link_with_libuuid" = "xyes"])
AM_CONDITIONAL([BABELTRACE_BUILD_WITH_LIBC_UUID], [test "x$link_with_libc_uuid" = "xyes"])

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

# Check for posix_fallocate
AC_CHECK_LIB([c], [posix_fallocate],
[
	AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_POSIX_FALLOCATE], 1, [Has posix_fallocate support.])
]
)

# Check for faccessat
AC_CHECK_LIB([c], [faccessat],
[
	AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_FACCESSAT], 1, [Has faccessat support.])
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
  AM_PATH_PYTHON([3.0], [], [ AC_MSG_ERROR([Python 3.0 or newer is needed]) ])

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

# Set default enable state for debug info.
# The _enable_debug_info variable is prepended with an underscore to
# avoid clashing with the one generated by AC_ARG_ENABLE.
AS_IF([test "x$DEFAULT_ENABLE_DEBUG_INFO" = xyes], [_enable_debug_info=yes], [_enable_debug_info=no])

# Optional debug info feature
# Do _not_ indent the help string below (appears in the configure --help
# output).
AC_ARG_ENABLE([debug-info],
[AC_HELP_STRING([--enable-debug-info], [enable the debug info feature (default on Linux)])]
[AC_HELP_STRING([--disable-debug-info], [disable the debug info feature (default on OS X and Solaris)])],
	[AS_IF([test "x$enableval" = xyes], [_enable_debug_info=yes], [_enable_debug_info=no])], [])

AM_CONDITIONAL([ENABLE_DEBUG_INFO], [test "x$_enable_debug_info" = xyes])
AS_IF([test "x$_enable_debug_info" = xyes], [
	    # Check if libelf and libdw are present
	    AC_CHECK_LIB([elf], [elf_version], [], [AC_MSG_ERROR(Missing libelf (from elfutils >= 0.154) which is required by debug info. You can disable this feature using --disable-debug-info.)])
	    AC_CHECK_LIB([dw], [dwarf_begin], [], [AC_MSG_ERROR(Missing libdw (from elfutils >= 0.154) which is required by debug info. You can disable this feature using --disable-debug-info.)])
	    BT_LIB_ELFUTILS([0], [154], [:], [AC_MSG_ERROR(elfutils >= 0.154 is required to use the debug info feature. You can disable this feature using --disable-debug-info.)])
            AC_DEFINE([ENABLE_DEBUG_INFO], [1], [Define to 1 if you enable the 'debug info' feature])
], [])

PKG_CHECK_MODULES(GMODULE, [gmodule-2.0 >= 2.0.0])
AC_SUBST(PACKAGE_LIBS)

LIBS="$LIBS $GMODULE_LIBS"
PACKAGE_CFLAGS="$GMODULE_CFLAGS -Wall -Wformat"
AC_SUBST(PACKAGE_CFLAGS)

DEFAULT_INCLUDES="-I\$(top_srcdir) -I\$(top_builddir) -I\$(top_builddir)/include -include config.h"
AC_SUBST(DEFAULT_INCLUDES)

babeltraceincludedir="${includedir}/babeltrace"
AC_SUBST(babeltraceincludedir)

babeltracectfincludedir="${includedir}/babeltrace/ctf"
AC_SUBST(babeltracectfincludedir)

babeltracectfwriterincludedir="${includedir}/babeltrace/ctf-writer"
AC_SUBST(babeltracectfwriterincludedir)

babeltracectfirincludedir="${includedir}/babeltrace/ctf-ir"
AC_SUBST(babeltracectfirincludedir)

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
	formats/ctf/ir/Makefile
	converter/Makefile
	doc/Makefile
	lib/Makefile
	lib/prio_heap/Makefile
	include/Makefile
	bindings/Makefile
	bindings/python/Makefile
	bindings/python/setup.py
	bindings/python/babeltrace/Makefile
	bindings/python/babeltrace/__init__.py
	tests/Makefile
	tests/bin/Makefile
	tests/bin/intersection/Makefile
	tests/lib/Makefile
	tests/utils/Makefile
	tests/utils/tap/Makefile
	extras/Makefile
	extras/valgrind/Makefile
	babeltrace.pc
	babeltrace-ctf.pc
])

AC_CONFIG_FILES([tests/lib/test_ctf_writer_complete], [chmod +x tests/lib/test_ctf_writer_complete])
AC_CONFIG_FILES([tests/lib/test_seek_big_trace], [chmod +x tests/lib/test_seek_big_trace])
AC_CONFIG_FILES([tests/lib/test_seek_empty_packet], [chmod +x tests/lib/test_seek_empty_packet])
AC_CONFIG_FILES([tests/lib/test_dwarf_complete], [chmod +x tests/lib/test_dwarf_complete])
AC_CONFIG_FILES([tests/lib/test_bin_info_complete], [chmod +x tests/lib/test_bin_info_complete])

AC_CONFIG_FILES([tests/bin/test_trace_read], [chmod +x tests/bin/test_trace_read])
AC_CONFIG_FILES([tests/bin/intersection/test_intersection], [chmod +x tests/bin/intersection/test_intersection])
AC_CONFIG_FILES([tests/bin/intersection/bt_python_helper.py])
AC_CONFIG_FILES([tests/bin/test_packet_seq_num], [chmod +x tests/bin/test_packet_seq_num])

# Create link for Babeltrace complements files for out-of-tree builds
AC_CONFIG_LINKS([
	bindings/python/babeltrace/python-complements.c:bindings/python/babeltrace/python-complements.c
	bindings/python/babeltrace/python-complements.h:bindings/python/babeltrace/python-complements.h
	bindings/python/babeltrace/babeltrace.i.in:bindings/python/babeltrace/babeltrace.i.in
])

AC_OUTPUT

#
# Mini-report on what will be built.
#

PPRINT_INIT
PPRINT_SET_INDENT(1)
PPRINT_SET_TS(38)

AS_ECHO
AS_ECHO("${PPRINT_COLOR_BLDBLU}Babeltrace $PACKAGE_VERSION$PPRINT_COLOR_RST")
AS_ECHO

PPRINT_SUBTITLE([Features])

# Target architecture we're building for.
target_arch=$host_cpu
[
for f in $CFLAGS; do
	if test $f = "-m32"; then
		target_arch="32-bit"
	elif test $f = "-m64"; then
		target_arch="64-bit"
	fi
done
]
PPRINT_PROP_STRING([Target architecture], $target_arch)

# python bindings enabled/disabled
test "x$enable_python" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python bindings], $value)

# python bindings doc enabled/disabled
test "x$enable_python_bindings_doc" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python bindings doc], $value)

# debug info enabled/disabled
test "x$_enable_debug_info" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Debug information output], $value)


report_bindir="`eval eval echo $bindir`"
report_libdir="`eval eval echo $libdir`"

# Print the bindir and libdir this `make install' will install into.
AS_ECHO
PPRINT_SUBTITLE([Install directories])
PPRINT_PROP_STRING([Binaries], [$report_bindir])
PPRINT_PROP_STRING([Libraries], [$report_libdir])
