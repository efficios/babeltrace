AM_CPPFLAGS = $(CPPFLAGS) -I$(top_srcdir)/include -I$(srcdir)
AM_CFLAGS = $(PACKAGE_CFLAGS)
BUILT_SOURCES = parser.h parser.c lexer.c
AM_YFLAGS = -t -d -v

noinst_LTLIBRARIES = libctf-parser.la libctf-ast.la

libctf_parser_la_SOURCES = lexer.l parser.y objstack.c
# ctf-scanner-symbols.h is included to prefix generated yy_* symbols
# with bt_.
libctf_parser_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir) \
		-include $(srcdir)/scanner-symbols.h

libctf_ast_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)
libctf_ast_la_SOURCES = \
	visitor-generate-ir.c \
	visitor-semantic-validator.c \
	visitor-parent-links.c \
	ast.h \
	objstack.h \
	parser.h \
	scanner.h \
	scanner-symbols.h \
	decoder.c \
	decoder.h

libctf_ast_la_LIBADD =

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_ast_la_LIBADD += -luuid
endif

if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_ast_la_LIBADD += -lc
endif

if BABELTRACE_BUILD_WITH_MINGW
libctf_ast_la_LIBADD += -lrpcrt4 -lintl -liconv -lole32 -lpopt
endif

CLEANFILES = $(BUILT_SOURCES) parser.output
