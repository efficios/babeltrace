dnl Process this file with autoconf to produce a configure script.
dnl
dnl Copyright (c) 2017 EfficiOS, Inc.
dnl
dnl Permission is hereby granted, free of charge, to any person obtaining a copy
dnl of this software and associated documentation files (the "Software"), to deal
dnl in the Software without restriction, including without limitation the rights
dnl to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
dnl copies of the Software, and to permit persons to whom the Software is
dnl furnished to do so, subject to the following conditions:
dnl
dnl The above copyright notice and this permission notice shall be included in
dnl all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
dnl IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
dnl AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
dnl LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
dnl OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
dnl SOFTWARE.

##                     ##
## Autoconf base setup ##
##                     ##

AC_PREREQ([2.50])

m4_define([bt_version_major], [2])
m4_define([bt_version_minor], [0])
m4_define([bt_version_patch], [0])
m4_define([bt_version_extra], [-pre4])
m4_define([bt_version], bt_version_major[.]bt_version_minor[.]bt_version_patch[]bt_version_extra)

AC_INIT([babeltrace], bt_version, [jeremie dot galarneau at efficios dot com], [], [https://efficios.com/babeltrace/])

# Following the numbering scheme proposed by libtool for the library version
# http://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
AC_SUBST([BABELTRACE_LIBRARY_VERSION], bt_version_major[:]bt_version_minor[:]bt_version_patch)

AC_CONFIG_HEADERS([include/config.h])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AC_REQUIRE_AUX_FILE([tap-driver.sh])

AC_DEFINE([BT_VERSION_MAJOR], bt_version_major, [Babeltrace library major version])
AC_DEFINE([BT_VERSION_MINOR], bt_version_minor, [Babeltrace library minor version])
AC_DEFINE([BT_VERSION_PATCH], bt_version_patch, [Babeltrace library patch version])
AC_DEFINE([BT_VERSION_EXTRA], ["]bt_version_extra["], [Babeltrace library extra version])

AC_CANONICAL_TARGET
AC_CANONICAL_HOST


##                     ##
## Automake base setup ##
##                     ##

AM_INIT_AUTOMAKE([foreign dist-bzip2 no-dist-gzip tar-ustar nostdinc])
AM_MAINTAINER_MODE([enable])

# Enable silent rules if available (Introduced in AM 1.11)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])


##                      ##
## OS specific defaults ##
##                      ##

MINGW32=no
DEFAULT_ENABLE_DEBUG_INFO=no
LT_NO_UNDEFINED=""
AS_CASE([$host_os],
  [solaris*|darwin*],
    [
	DEFAULT_ENABLE_DEBUG_INFO=no
    ],
  [cygwin*],
    [
	DEFAULT_ENABLE_DEBUG_INFO=no
	LT_NO_UNDEFINED="-no-undefined"
    ],
  [mingw*],
    [
	MINGW32=yes
	DEFAULT_ENABLE_DEBUG_INFO=no
	LT_NO_UNDEFINED="-no-undefined"
    ]
)

AM_CONDITIONAL([BABELTRACE_BUILD_WITH_MINGW], [test "x$MINGW32" = "xyes"])
AC_SUBST(LT_NO_UNDEFINED)


##                   ##
## C compiler checks ##
##                   ##

AC_USE_SYSTEM_EXTENSIONS
AC_SYS_LARGEFILE

# Choose the c compiler
AC_PROG_CC

# Make sure the c compiler supports C99
AC_PROG_CC_C99([], [AC_MSG_ERROR([The compiler does not support C99])])

# Make sure the c compiler supports __attributes__
AX_C___ATTRIBUTE__
AS_IF([test "x$ax_cv___attribute__" != "xyes"],
  [AC_MSG_ERROR([The compiler does not support __attribute__ extensions])])

# Make sur we have pthread support
AX_PTHREAD([], [AC_MSG_ERROR([Could not configure pthread support])])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_TYPEOF
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_INT8_T
AC_TYPE_OFF_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])


##               ##
## Header checks ##
##               ##

AC_HEADER_STDBOOL
AC_CHECK_HEADERS([ \
	fcntl.h \
	float.h \
	ftw.h \
	libintl.h \
	limits.h \
	malloc.h \
	netdb.h \
	netinet/in.h \
	stddef.h \
	sys/socket.h \
	sys/time.h
])


##               ##
## Linker checks ##
##               ##

# Check if the linker supports no-as-needed
AX_APPEND_LINK_FLAGS([-Wl,--no-as-needed], [LD_NO_AS_NEEDED])
AC_SUBST([LD_NO_AS_NEEDED])

# Check if the linker supports whole-archive
AX_CHECK_LINK_FLAG([-Wl,--whole-archive,--no-whole-archive],
  [
    AC_SUBST([LD_WHOLE_ARCHIVE], [-Wl,--whole-archive,])
    AC_SUBST([LD_NO_WHOLE_ARCHIVE], [,--no-whole-archive])
  ],[
    # Fallback to force_load for the macOS linker
    AX_CHECK_LINK_FLAG([-Wl,-force_load],
      [
        AC_SUBST([LD_WHOLE_ARCHIVE], [-Wl,-force_load,])
        AC_SUBST([LD_NO_WHOLE_ARCHIVE], [])
      ],[
        AC_MSG_WARN([Can't find a linker option to force the inclusion of the static plugin archive objects.])
      ]
    )
  ]
)

# Initialize and configure libtool
LT_INIT([win32-dll])


##                 ##
## Programs checks ##
##                 ##

AC_PROG_MAKE_SET
AC_PROG_MKDIR_P
AC_PROG_LN_S

# set $IN_GIT_REPO if we're in the Git repository; the `bootstrap` file
# is not distributed in tarballs
AS_IF([test -f "$srcdir/bootstrap"], [in_git_repo=yes], [in_git_repo=no])
AM_CONDITIONAL([IN_GIT_REPO], [test "x$in_git_repo" = "xyes"])

# check for bison
AC_PROG_YACC
AX_PROG_BISON_VERSION([2.4], [have_bison=yes])

AS_IF([test "x$have_bison" != "xyes"], [
  AS_IF([test "x$in_git_repo" = "xyes"], [
    AC_MSG_FAILURE([dnl
Bison >= 2.4 is required when building from the Git repository. You can
set the YACC variable to override automatic detection.
    ])
  ], [
    AC_MSG_WARN([dnl
Missing Bison >= 2.4. Note that the parser files are already built in
this distribution tarball, so Bison is only needed if you intend to
modify their sources. You can set the YACC variable to override automatic
detection.
    ])
  ])
])
AM_CONDITIONAL([HAVE_BISON], [test "x$have_bison" = "xyes"])

# check for flex
AC_PROG_LEX
AX_PROG_FLEX_VERSION([2.5.35], [have_flex=yes])

AS_IF([test "x$have_flex" != "xyes"], [
  AS_IF([test "x$in_git_repo" = "xyes"], [
    AC_MSG_FAILURE([dnl
Flex >= 2.5.35 is required when building from the Git repository. You can
set the LEX variable to override automatic detection.
    ])
  ], [
    AC_MSG_WARN([dnl
Missing Flex >= 2.5.35. Note that the lexer files are already built in
this distribution tarball, so Flex is only needed if you intend to
modify their sources. You can set the LEX variable to override automatic
detection.
    ])
  ])
])
AM_CONDITIONAL([HAVE_FLEX], [test "x$have_flex" = "xyes"])


##                ##
## Library checks ##
##                ##

# Check what libraries are required on this platform to link sockets programs.
AX_LIB_SOCKET_NSL

# Check for glib >= 2.22 with gmodule support
AM_PATH_GLIB_2_0([2.22.0], [],
  AC_MSG_ERROR([glib >= 2.22 is required - download it from ftp://ftp.gtk.org/pub/gtk]),
  [gmodule-no-export]
)

# Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_FORK
AC_FUNC_MKTIME
AC_FUNC_MMAP
AC_FUNC_STRERROR_R
AC_FUNC_STRNLEN
AC_CHECK_FUNCS([ \
	atexit \
	dup2 \
	ftruncate \
	gethostbyname \
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
	strstr \
	strtoul \
	strtoull \
	tzset \
])

# AC_FUNC_MALLOC causes problems when cross-compiling.
#AC_FUNC_MALLOC
#AC_FUNC_REALLOC

# First, check for uuid in system libs
AH_TEMPLATE([BABELTRACE_HAVE_LIBUUID], [Define if you have libuuid support])
AC_CHECK_FUNCS([uuid_generate],
  [
    AC_DEFINE([BABELTRACE_HAVE_LIBUUID], [1])
    UUID_LIBS=""
  ],
  [
    # Then, check if the pkg-config module is available, otherwise explicitly check
    # for libuuid, or uuid support in the C-library.
    PKG_CHECK_MODULES([UUID], [uuid],
      [
        AC_DEFINE([BABELTRACE_HAVE_LIBUUID], [1])
        dnl PKG_CHECK_MODULES defines UUID_LIBS
      ],
      [
        AC_MSG_WARN([pkg-config was unable to find a valid .pc for libuuid. Set PKG_CONFIG_PATH to specify the pkgconfig configuration file location])
        AC_MSG_WARN([Finding libuuid without pkg-config.])
        AC_CHECK_LIB([uuid], [uuid_generate],
          [
            AC_DEFINE([BABELTRACE_HAVE_LIBUUID], [1])
            UUID_LIBS="-luuid"
          ],
          [
            # libuuid not found, check for uuid_create in libc.
            AC_CHECK_LIB([c], [uuid_create],
              [
                AC_DEFINE([BABELTRACE_HAVE_LIBUUID], [1])
                UUID_LIBS="-lc"
              ],
              [
                # for MinGW32 we have our own internal implementation of uuid using Windows functions.
                AS_IF([test "x$MINGW32" = xno],
                  [AC_MSG_FAILURE([Cannot find libuuid uuid_generate nor libc uuid_create. Either set PKG_CONFIG_PATH to the configuration file location or use LDFLAGS=-Ldir to specify the library location])]
		)
              ]
            )
          ]
        )
      ]
    )
  ]
)
AC_SUBST(UUID_LIBS)

# Check for fmemopen
AC_CHECK_LIB([c], [fmemopen],
  [AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_FMEMOPEN], 1, [Has fmemopen support.])]
)

# Check for open_memstream
AC_CHECK_LIB([c], [open_memstream],
  [AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_OPEN_MEMSTREAM], 1, [Has open_memstream support.])]
)

# Check for posix_fallocate
AC_CHECK_LIB([c], [posix_fallocate],
  [AC_DEFINE_UNQUOTED([BABELTRACE_HAVE_POSIX_FALLOCATE], 1, [Has posix_fallocate support.])]
)

# Check libpopt
PKG_CHECK_MODULES([POPT], [popt],
  [
    dnl PKG_CHECK_MODULES defines POPT_LIBS
  ],
  [
    AC_MSG_WARN([pkg-config was unable to find a valid .pc for libpopt. Set PKG_CONFIG_PATH to specify the pkgconfig configuration file location])
    AC_MSG_WARN([Finding libpopt without pkg-config.])
    AC_CHECK_LIB([popt],
      [poptGetContext],
      [POPT_LIBS="-lpopt"],
      [
        AC_MSG_FAILURE([Cannot find libpopt. Either set PKG_CONFIG_PATH to the configuration file location or use LDFLAGS=-Ldir to specify the library location])
      ]
    )
  ]
)
AC_SUBST(POPT_LIBS)


##                 ##
## User variables  ##
##                 ##

AC_ARG_VAR([PYTHON_INCLUDE], [Include flags for Python, bypassing python-config])
AC_ARG_VAR([PYTHON_LIBS], [Library flags for Python, bypassing python-config])
AC_ARG_VAR([PYTHON_CONFIG], [Path to python-config])

# PLUGINSDIR: Plugins directory
AC_ARG_VAR([PLUGINSDIR], [built-in plugins install directory [LIBDIR/babeltrace/plugins]])
AS_IF([test "x$PLUGINSDIR" = x], [PLUGINSDIR='$(libdir)/babeltrace/plugins'])

# BABELTRACE_MINIMAL_LOG_LEVEL:
AC_ARG_VAR([BABELTRACE_MINIMAL_LOG_LEVEL], [Minimal log level for Babeltrace program, library, and plugins (VERBOSE, DEBUG, INFO, WARN, ERROR (default), FATAL, or NONE)])
AS_IF([test "x$BABELTRACE_MINIMAL_LOG_LEVEL" = x], [BABELTRACE_MINIMAL_LOG_LEVEL="VERBOSE"])
AS_IF([test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "VERBOSE" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "DEBUG" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "INFO" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "WARN" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "ERROR" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "FATAL" && \
       test "$BABELTRACE_MINIMAL_LOG_LEVEL" != "NONE"],
  [AC_MSG_ERROR([Invalid BABELTRACE_MINIMAL_LOG_LEVEL value ($BABELTRACE_MINIMAL_LOG_LEVEL): use VERBOSE, DEBUG, INFO, WARN, ERROR, FATAL, or NONE.])]
)
AC_DEFINE_UNQUOTED([BT_LOG_LEVEL], [BT_LOG_$BABELTRACE_MINIMAL_LOG_LEVEL], [Minimal log level])

# BABELTRACE_DEV_MODE:
AC_ARG_VAR([BABELTRACE_DEV_MODE], [Set to 1 to enable the Babeltrace developer mode (enables run-time checks for plugin developers)])
AS_IF([test "x$BABELTRACE_DEV_MODE" = x1], [
	AS_IF([test "x$BABELTRACE_MINIMAL_LOG_LEVEL" = "xNONE"], [
		AC_MSG_ERROR([Babeltrace developer mode (\$BABELTRACE_DEV_MODE) needs \$BABELTRACE_MINIMAL_LOG_LEVEL to be at least FATAL.])
	])
	AC_DEFINE([BT_DEV_MODE], 1, [Babeltrace developer mode])
], [BABELTRACE_DEV_MODE=0])

# BABELTRACE_DEBUG_MODE:
AC_ARG_VAR([BABELTRACE_DEBUG_MODE], [Set to 1 to enable the Babeltrace debug mode (enables internal assertions for Babeltrace maintainers)])
AS_IF([test "x$BABELTRACE_DEBUG_MODE" = x1], [
	AC_DEFINE([BT_DEBUG_MODE], 1, [Babeltrace debug mode])
], [BABELTRACE_DEBUG_MODE=0])


##                              ##
## Optionnal features selection ##
##                              ##

# Python bindings
# Disabled by default
AC_ARG_ENABLE([python-bindings],
  [AC_HELP_STRING([--enable-python-bindings], [build the Python bindings])],
  [], dnl AC_ARG_ENABLE will fill enable_python_bindings with the user choice
  [enable_python_bindings=no]
)

# Python bindings documentation
# Disabled by default
AC_ARG_ENABLE([python-bindings-doc],
  [AC_HELP_STRING([--enable-python-bindings-doc], [build the Python bindings documentation])],
  [], dnl AC_ARG_ENABLE will fill enable_python_bindings_doc with the user choice
  [enable_python_bindings_doc=no]
)

# Python plugins
# Disabled by default
AC_ARG_ENABLE([python-plugins],
  [AC_HELP_STRING([--enable-python-plugins], [enable the Python plugins support for the library and converter])],
  [], dnl AC_ARG_ENABLE will fill enable_python_plugins with the user choice
  [enable_python_plugins=no]
)

# Debug info
# Enabled by default, except on some platforms
AC_ARG_ENABLE([debug-info],
  [AC_HELP_STRING([--disable-debug-info], [disable the debug info support (default on macOS, Solaris and Windows)])],
  [], dnl AC_ARG_ENABLE will fill enable_debug_info with the user choice
  [enable_debug_info="$DEFAULT_ENABLE_DEBUG_INFO"]
)

# API documentation
# Disabled by default
AC_ARG_ENABLE([api-doc],
  [AC_HELP_STRING([--enable-api-doc], [build the HTML API documentation])],
  [enable_api_doc=$enableval],
  [enable_api_doc=no]
)

# Built-in plugins
# Disabled by default
AC_ARG_ENABLE([built-in-plugins],
  [AC_HELP_STRING([--enable-built-in-plugins], [Statically-link in-tree plug-ins into the babeltrace binary])],
  [], dnl AC_ARG_ENABLE will fill enable_built_in_plugins with the user choice
  [enable_built_in_plugins=no]
)

# Built-in python plugin support
# Disabled by default
AC_ARG_ENABLE([built-in-python-plugin-support],
  [AC_HELP_STRING([--enable-built-in-python-plugin-support], [Statically-link Python plugin support into the babeltrace library])],
  [], dnl AC_ARG_ENABLE will fill enable_built_in_python_plugin_support with the user choice
  [enable_built_in_python_plugin_support=no]
)

# Man pages
# Enabled by default
AC_ARG_ENABLE([man-pages],
  [AS_HELP_STRING([--disable-man-pages], [Do not build and install man pages (already built in a distributed tarball])],
  [], dnl AC_ARG_ENABLE will fill enable_built_in_plugins with the user choice
  [enable_man_pages=yes]
)


# Set automake variables for optionnal feature conditionnals in Makefile.am
AM_CONDITIONAL([ENABLE_PYTHON_BINDINGS], [test "x$enable_python_bindings" = xyes])
AM_CONDITIONAL([ENABLE_PYTHON_BINDINGS_DOC], [test "x$enable_python_bindings_doc" = xyes])
AM_CONDITIONAL([ENABLE_PYTHON_PLUGINS], [test "x$enable_python_plugins" = xyes])
AM_CONDITIONAL([ENABLE_DEBUG_INFO], [test "x$enable_debug_info" = xyes])
AM_CONDITIONAL([ENABLE_API_DOC], [test "x$enable_api_doc" = xyes])
AM_CONDITIONAL([ENABLE_BUILT_IN_PLUGINS], [test "x$enable_built_in_plugins" = xyes])
AM_CONDITIONAL([ENABLE_BUILT_IN_PYTHON_PLUGIN_SUPPORT], [test "x$enable_built_in_python_plugin_support" = xyes])
AM_CONDITIONAL([ENABLE_MAN_PAGES], [test "x$enable_man_pages" = xyes])


# Set defines for optionnal features conditionnals in the source code

## jgalar: still used?
AS_IF([test "x$enable_python_plugins" = xyes],
  [AC_DEFINE([PYTHON_PLUGINS], [1], [Python plugin support.])]
)

AS_IF([test "x$enable_debug_info" = xyes],
  [AC_DEFINE([ENABLE_DEBUG_INFO], [1], [Define to 1 if you enable the 'debug info' feature])]
)

AS_IF([test "x$enable_built_in_plugins" = xyes],
  [AC_DEFINE([BT_BUILT_IN_PLUGINS], [1], [Define to 1 to register plug-in attributes in static executable sections])]
)

AS_IF([test "x$enable_built_in_python_plugin_support" = xyes],
  [AC_DEFINE([BT_BUILT_IN_PYTHON_PLUGIN_SUPPORT], [1], [Define to 1 to register plug-in attributes in static executable sections])]
)

AS_IF([test "x$enable_debug_info" = xyes],
  [ENABLE_DEBUG_INFO_VAL=1],
  [ENABLE_DEBUG_INFO_VAL=0]
)

AC_SUBST([ENABLE_DEBUG_INFO_VAL])


# Check for conflicting optional features user choices

AS_IF([test "x$enable_python_bindings" = xno],
  [
    AS_IF([test "x$enable_python_bindings_doc" = xyes], [AC_MSG_ERROR([--enable-python-bindings-doc was specified without --enable-python-bindings])])
  ]
)

AS_IF([test "x$enable_built_in_plugins" = xyes],
  [
    # Built-in plug-ins are only available when the --disable-shared --enable-static options are used.
    AS_IF([test "x$enable_static" != xyes], [AC_MSG_ERROR(--enable-static must be used to bundle plug-ins in the babeltrace executable)])
    AS_IF([test "x$enable_shared" = xyes], [AC_MSG_ERROR(--disable-shared must be used to bundle plug-ins in the babeltrace executable)])
  ]
)

AS_IF([test "x$enable_built_in_python_plugin_support" = xyes],
  [
    AS_IF([test "x$enable_python_plugins" = xno], [AC_MSG_ERROR([--enable-python-plugins must be used to bundle Python plugin support in the babeltrace executable])])
    # Built-in plug-ins are only available when the --disable-shared --enable-static options are used.
    AS_IF([test "x$enable_static" != xyes], [AC_MSG_ERROR(--enable-static must be used to bundle Python plugin support in the babeltrace executable)])
    AS_IF([test "x$enable_shared" = xyes], [AC_MSG_ERROR(--disable-shared must be used to bundle Python plugin support in the babeltrace executable)])
  ]
)


# Check for optionnal features dependencies

AS_IF([test "x$enable_python_bindings" = xyes],
  [AX_PKG_SWIG(2.0.0, [], [AC_MSG_ERROR([SWIG 2.0.0 or newer is required to build the python bindings])])]
)

AS_IF([test "x$enable_python_bindings" = xyes || test "x$enable_python_plugins" = xyes],
  [
  AM_PATH_PYTHON([3.0], [], [AC_MSG_ERROR(Python 3 is not available or is not the default Python interpreter on your system. See the README file to learn how to override your distribution's default Python interpreter.)])

  AM_PATH_PYTHON_MODULES([PYTHON])
  # pythondir is the path where extra modules are to be installed
  pythondir=$PYTHON_PREFIX/$PYTHON_MODULES_PATH
  # pyexecdir is the path that contains shared objects used by the extra modules
  pyexecdir=$PYTHON_EXEC_PREFIX/$PYTHON_MODULES_PATH
  AS_IF([test -z "$PYTHON_INCLUDE"], [
    AS_IF([test -z "$PYTHON_CONFIG"], [
      AC_PATH_PROGS([PYTHON_CONFIG],
                    [python$PYTHON_VERSION-config python-config],
                    [no],
                    [`dirname $PYTHON`])
      AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON. Is python-dev installed?])])
    ])
    AC_MSG_CHECKING([Python include flags])
    PYTHON_INCLUDE=`$PYTHON_CONFIG --includes`
    AC_MSG_RESULT([$PYTHON_INCLUDE])
  ])
  AS_IF([test -z "$PYTHON_LIBS"], [
    AS_IF([test -z "$PYTHON_CONFIG"], [
      AC_PATH_PROGS([PYTHON_CONFIG],
                    [python$PYTHON_VERSION-config python-config],
                    [no],
                    [`dirname $PYTHON`])
      AS_IF([test "$PYTHON_CONFIG" = no], [AC_MSG_ERROR([cannot find python-config for $PYTHON. Is python-dev installed?])])
    ])
    AC_MSG_CHECKING([Python library flags])
    PYTHON_LIBS=`$PYTHON_CONFIG --libs`
    AC_MSG_RESULT([$PYTHON_LIBS])
  ])
  ]
)

AS_IF([test "x$enable_python_bindings_doc" = xyes],
  [
    AM_CHECK_PYTHON_SPHINX([PYTHON])
    AS_IF([test "x$PYTHON_SPHINX_EXISTS" = xno],
      [AC_MSG_ERROR([The Sphinx package for Python 3 is required to build the Python bindings documentation])]
    )
  ]
)

AS_IF([test "x$enable_debug_info" = xyes],
  [
    # Check if libelf and libdw are present
    AC_CHECK_LIB([elf], [elf_version], [:], [AC_MSG_ERROR(Missing libelf (from elfutils >= 0.154) which is required by debug info. You can disable this feature using --disable-debug-info.)])
    AC_CHECK_LIB([dw], [dwarf_begin], [:], [AC_MSG_ERROR(Missing libdw (from elfutils >= 0.154) which is required by debug info. You can disable this feature using --disable-debug-info.)])
    BT_LIB_ELFUTILS([0], [154], [:], [AC_MSG_ERROR(elfutils >= 0.154 is required to use the debug info feature. You can disable this feature using --disable-debug-info.)])
    ELFUTILS_LIBS="-lelf -ldw"
  ]
)
AC_SUBST([ELFUTILS_LIBS])

AS_IF([test "x$enable_api_doc" = "xyes"],
  [
    DX_DOXYGEN_FEATURE(ON)
    DX_DOT_FEATURE(OFF)
    DX_HTML_FEATURE(ON)
    DX_CHM_FEATURE(OFF)
    DX_CHI_FEATURE(OFF)
    DX_MAN_FEATURE(OFF)
    DX_RTF_FEATURE(OFF)
    DX_XML_FEATURE(OFF)
    DX_PDF_FEATURE(OFF)
    DX_PS_FEATURE(OFF)
    DX_INIT_DOXYGEN([Babeltrace], [$(srcdir)/Doxyfile], [output])
    AS_IF([test -z "$DX_DOXYGEN"],
	  [AC_MSG_ERROR([You need doxygen to enable the API documentation])]
    )
  ]
)

have_asciidoc_xmlto=no
warn_prebuilt_man_pages=no

AS_IF([test "x$enable_man_pages" = "xyes"], [
  AC_PATH_PROG([ASCIIDOC], [asciidoc], [no])
  AC_PATH_PROG([XMLTO], [xmlto], [no])

  AS_IF([test "x$ASCIIDOC" = "xno" || test "x$XMLTO" = "xno"], [
    AS_IF([test "x$in_git_repo" = "xyes"], [
      # this is an error because we're in the Git repo, which
      # means the man pages are not already generated for us,
      # thus asciixmlto are required because we were asked
      # to build the man pages
      AC_MSG_ERROR([
You need asciidoc and xmlto to build the Babeltrace man pages. Use
--disable-man-pages to disable building the man pages, in which case
they will not be installed.
      ])
    ], [
      # only warn here: since we're in the tarball, the man
      # pages should already be generated at this point, thus
      # asciidoc/xmlto are not strictly required
      warn_prebuilt_man_pages=yes
    ])
  ], [
    have_asciidoc_xmlto=yes
  ])
])

# export AsciiDoc and xmlto existence
AM_CONDITIONAL([HAVE_ASCIIDOC_XMLTO], [test "x$have_asciidoc_xmlto" = "xyes"])

# a wonderful hack that seems necessary because $libdir is
# literally `${exec_prefix}/lib`, and $exec_prefix is set to `NONE`
# by autoconf when it's not specified by the user
AS_IF([test "x$exec_prefix" = xNONE], [
  AS_IF([test "x$prefix" = xNONE], [
    PREFIX="$ac_default_prefix"
  ], [
    PREFIX="$prefix"
  ])

  LIBDIR="$PREFIX/lib"
], [
  LIBDIR="$libdir"
])

AC_SUBST(LIBDIR)


# Set global CFLAGS in AM_CFLAGS
AM_CFLAGS="-Wall -Wformat $PTHREAD_CFLAGS $GLIB_CFLAGS"
AC_SUBST(AM_CFLAGS)

# Set global CPPFLAGS in AM_CPPFLAGS
AM_CPPFLAGS="-I\$(top_builddir)/include -I\$(top_srcdir)/include -include config.h"
AC_SUBST(AM_CPPFLAGS)

# Add glib to global link libs
LIBS="$LIBS $GLIB_LIBS"

# Check that the current size_t matches the size that glib thinks it should
# be. This catches problems on multi-arch where people try to do a 32-bit
# build while pointing at 64-bit glib headers. This is a common error because
# glib.h is not platform specific but it includes glibconfig.h which is and
# is usually installed in a non-standard path.

# Older versions of the pkg-config macros disallows PKG_* in the autoconf
# output. Specifically allow pkg_config_libdir to be able to print the
# error message.
m4_pattern_allow([PKG_CONFIG_LIBDIR])

save_CFLAGS=${CFLAGS}
CFLAGS="${CFLAGS} ${AM_CFLAGS}"
AC_COMPILE_IFELSE([
  AC_LANG_PROGRAM([dnl
#include <glib.h>
#include <unistd.h>
  ], [dnl
G_STATIC_ASSERT(sizeof(size_t) == GLIB_SIZEOF_SIZE_T);
  ])
],[:],[
  AC_MSG_ERROR([dnl
sizeof(size_t) doesn't match GLIB_SIZEOF_SIZE_T. You probably need to set
PKG_CONFIG_LIBDIR to point to the right pkg-config files for your build
target.
  ])
])
CFLAGS=${save_CFLAGS}

# Abuse autoconf's AC_ARG_PROGRAM output variable 'program_transform_name'
# to rename babeltrace.bin to babeltrace at install time.
program_transform_name="s&babeltrace\.bin&babeltrace&;s&babeltrace-log\.bin&babeltrace-log&;$program_transform_name"
AC_SUBST(program_transform_name)

AC_CONFIG_FILES([
	Makefile
	bindings/python/Makefile
	bindings/python/babeltrace/Makefile
	bindings/python/babeltrace/setup.py
	bindings/python/babeltrace/babeltrace/__init__.py
	bindings/python/bt2/Makefile
	bindings/python/bt2/setup.py
	bindings/python/bt2/bt2/__init__.py
	common/Makefile
	ctfser/Makefile
	compat/Makefile
	cli/Makefile
	doc/Makefile
	doc/api/Makefile
	doc/api/Doxyfile
	doc/bindings/Makefile
	doc/bindings/python/Makefile
	doc/contributing-images/Makefile
  doc/man/Makefile
  doc/man/asciidoc-attrs.conf
	lib/Makefile
	lib/prio_heap/Makefile
	lib/plugin/Makefile
	lib/graph/Makefile
	lib/graph/message/Makefile
	lib/trace-ir/Makefile
	lib/ctf-writer/Makefile
	include/Makefile
	logging/Makefile
	bindings/Makefile
	tests/Makefile
	tests/cli/Makefile
	tests/cli/intersection/Makefile
	tests/lib/Makefile
	tests/lib/test-plugin-plugins/Makefile
	tests/lib/trace-ir/Makefile
	tests/lib/ctf-writer/Makefile
	tests/utils/common.sh
	tests/utils/Makefile
	tests/utils/tap/Makefile
	tests/bindings/Makefile
	tests/bindings/python/Makefile
	tests/bindings/python/bt2/Makefile
	tests/bindings/python/babeltrace/Makefile
	tests/plugins/Makefile
	tests/python-plugin-provider/Makefile
	extras/Makefile
	extras/valgrind/Makefile
	plugins/Makefile
	plugins/ctf/Makefile
	plugins/ctf/common/Makefile
	plugins/ctf/common/bfcr/Makefile
	plugins/ctf/common/metadata/Makefile
	plugins/ctf/common/msg-iter/Makefile
	plugins/ctf/common/utils/Makefile
	plugins/ctf/fs-src/Makefile
	plugins/ctf/fs-sink/Makefile
	plugins/ctf/lttng-live/Makefile
	plugins/text/Makefile
	plugins/text/dmesg/Makefile
	plugins/text/pretty/Makefile
	plugins/utils/Makefile
	plugins/utils/dummy/Makefile
	plugins/utils/counter/Makefile
	plugins/utils/trimmer/Makefile
	plugins/utils/muxer/Makefile
	python-plugin-provider/Makefile
	plugins/libctfcopytrace/Makefile
	plugins/lttng-utils/Makefile
	babeltrace.pc
	babeltrace-ctf.pc
])

AC_CONFIG_FILES([tests/cli/intersection/test_intersection], [chmod +x tests/cli/intersection/test_intersection])
AC_CONFIG_FILES([tests/cli/test_convert_args], [chmod +x tests/cli/test_convert_args])
AC_CONFIG_FILES([tests/cli/test_packet_seq_num], [chmod +x tests/cli/test_packet_seq_num])
AC_CONFIG_FILES([tests/cli/test_trace_copy], [chmod +x tests/cli/test_trace_copy])
AC_CONFIG_FILES([tests/cli/test_trace_read], [chmod +x tests/cli/test_trace_read])
AC_CONFIG_FILES([tests/cli/test_trimmer], [chmod +x tests/cli/test_trimmer])
AC_CONFIG_FILES([tests/lib/test_ctf_writer_complete], [chmod +x tests/lib/test_ctf_writer_complete])
AC_CONFIG_FILES([tests/lib/test_plugin_complete], [chmod +x tests/lib/test_plugin_complete])
AC_CONFIG_FILES([tests/lib/trace-ir/test_trace_ir], [chmod +x tests/lib/trace-ir/test_trace_ir])
AC_CONFIG_FILES([tests/lib/ctf-writer/test_ctf_writer], [chmod +x tests/lib/ctf-writer/test_ctf_writer])
AC_CONFIG_FILES([tests/plugins/test-utils-muxer-complete], [chmod +x tests/plugins/test-utils-muxer-complete])
AC_CONFIG_FILES([tests/plugins/test_lttng_utils_debug_info], [chmod +x tests/plugins/test_lttng_utils_debug_info])
AC_CONFIG_FILES([tests/plugins/test_dwarf_complete], [chmod +x tests/plugins/test_dwarf_complete])
AC_CONFIG_FILES([tests/plugins/test_bin_info_complete], [chmod +x tests/plugins/test_bin_info_complete])

AS_IF([test "x$enable_python_bindings" = xyes],
  [
	AC_CONFIG_FILES([tests/bindings/python/bt2/test_python_bt2], [chmod +x tests/bindings/python/bt2/test_python_bt2])
	AC_CONFIG_FILES([tests/bindings/python/babeltrace/test_python_babeltrace], [chmod +x tests/bindings/python/babeltrace/test_python_babeltrace])
  ]
)

AS_IF([test "x$enable_python_plugins" = "xyes"],
  [AC_CONFIG_FILES([tests/python-plugin-provider/test_python_plugin_provider], [chmod +x tests/python-plugin-provider/test_python_plugin_provider])]
)

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

PPRINT_SUBTITLE([System])

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

AS_ECHO
PPRINT_SUBTITLE([Python 3 language support])
test "x$enable_python_bindings" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python bindings], $value)
test "x$enable_python_plugins" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python plugin support], $value)
AS_IF([test "x$enable_python_bindings" = "xyes" || test "x$enable_python_plugins" = "xyes"], [
  PPRINT_PROP_STRING([Python include paths], [$PYTHON_INCLUDE])
  PPRINT_PROP_STRING([Python libraries], [$PYTHON_LIBS])
])

AS_ECHO
PPRINT_SUBTITLE([Plugins])
PPRINT_PROP_BOOL(['ctf' plugin], 1)
test "x$enable_debug_info" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL(['lttng-utils' plugin], $value)
PPRINT_PROP_BOOL(['text' plugin], 1)
PPRINT_PROP_BOOL(['utils' plugin], 1)

AS_ECHO
PPRINT_SUBTITLE([Built-in features])
test "x$enable_built_in_plugins" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Built-in plugins], $value)
test "x$enable_built_in_python_plugin_support" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Built-in Python plugin support], $value)

AS_ECHO
PPRINT_SUBTITLE([Documentation])

# man pages build enabled/disabled
m4_pushdef([build_man_pages_msg], [Build and install man pages])

AS_IF([test "x$enable_man_pages" != "xno"], [
  AS_IF([test "x$in_git_repo" = "xyes"], [
    PPRINT_PROP_BOOL([build_man_pages_msg], 1)
  ], [
    AS_IF([test "x$have_asciidoc_xmlto" = "xyes"], [
      PPRINT_PROP_BOOL([build_man_pages_msg], 1)
    ], [
      PPRINT_PROP_STRING([build_man_pages_msg],
        [${PPRINT_COLOR_BLDGRN}yes (already built)],
        $PPRINT_COLOR_SUBTITLE)
    ])
  ])
], [
  PPRINT_PROP_BOOL([build_man_pages_msg], 0)
])

m4_popdef([build_man_pages_msg])

test "x$enable_api_doc" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([HTML API documentation], $value)
test "x$enable_python_bindings_doc" = "xyes" && value=1 || value=0
PPRINT_PROP_BOOL([Python bindings documentation], $value)

AS_ECHO
PPRINT_SUBTITLE([Logging])
PPRINT_PROP_STRING([Minimal log level], $BABELTRACE_MINIMAL_LOG_LEVEL)

AS_ECHO
PPRINT_SUBTITLE([Special build modes])
PPRINT_PROP_BOOL([Debug mode], $BABELTRACE_DEBUG_MODE)
PPRINT_PROP_BOOL([Developer mode], $BABELTRACE_DEV_MODE)

report_bindir="`eval eval echo $bindir`"
report_libdir="`eval eval echo $libdir`"
report_sysconfdif="`eval eval echo $sysconfdir`"

# Print the bindir and libdir this `make install' will install into.
AS_ECHO
PPRINT_SUBTITLE([Install directories])
PPRINT_PROP_STRING([Binaries], [$report_bindir])
PPRINT_PROP_STRING([Libraries], [$report_libdir])
PPRINT_PROP_STRING([Plugins], [$report_libdir/babeltrace/plugins])
PPRINT_PROP_STRING([Configuration], [$report_sysconfdif])
