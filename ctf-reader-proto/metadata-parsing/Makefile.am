AM_CPPFLAGS = $(CPPFLAGS) -I$(top_srcdir)/include -I$(srcdir)
AM_CFLAGS = $(PACKAGE_CFLAGS)
BUILT_SOURCES = ctf-parser.h ctf-parser.c ctf-lexer.c
AM_YFLAGS = -t -d -v

noinst_LTLIBRARIES = libctf-parser.la libctf-ast.la

noinst_HEADERS = \
	ctf-scanner.h \
	ctf-ast.h \
	ctf-scanner-symbols.h

libctf_parser_la_SOURCES = ctf-lexer.l ctf-parser.y objstack.c
# ctf-scanner-symbols.h is included to prefix generated yy_* symbols
# with bt_.
libctf_parser_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(srcdir) \
		-include $(srcdir)/ctf-scanner-symbols.h

libctf_ast_la_CPPFLAGS = $(AM_CPPFLAGS) -I$(builddir)
libctf_ast_la_SOURCES = \
		ctf-visitor-generate-ir.c \
		ctf-visitor-semantic-validator.c \
		ctf-visitor-parent-links.c
libctf_ast_la_LIBADD = $(top_builddir)/lib/libbabeltrace.la

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_ast_la_LIBADD += -luuid
endif

if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_ast_la_LIBADD += -lc
endif

if BABELTRACE_BUILD_WITH_MINGW
libctf_ast_la_LIBADD += -lrpcrt4 -lintl -liconv -lole32 -lpopt
endif

CLEANFILES = $(BUILT_SOURCES) ctf-parser.output
