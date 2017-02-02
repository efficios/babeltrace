AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include \
		-I$(srcdir)
BUILT_SOURCES = ctf-parser.h
AM_YFLAGS = -t -d -v

noinst_LTLIBRARIES = libctf-parser.la libctf-ast.la

noinst_HEADERS = \
	ctf-scanner.h \
	ctf-ast.h \
	ctf-scanner-symbols.h \
	objstack.h

libctf_parser_la_SOURCES = ctf-lexer.l ctf-parser.y objstack.c
# ctf-scanner-symbols.h is included to prefix generated yy_* symbols
# with bt_.
libctf_parser_la_CFLAGS = $(AM_CFLAGS) -I$(srcdir) \
		-include $(srcdir)/ctf-scanner-symbols.h
libctf_parser_la_LDFLAGS = $(LT_NO_UNDEFINED)

libctf_ast_la_CFLAGS = $(AM_CFLAGS) -I$(builddir)
libctf_ast_la_SOURCES = ctf-visitor-xml.c \
		ctf-visitor-parent-links.c \
		ctf-visitor-semantic-validator.c \
		ctf-visitor-generate-io-struct.c
libctf_ast_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la
libctf_ast_la_LDFLAGS = $(LT_NO_UNDEFINED)

if BABELTRACE_BUILD_WITH_LIBUUID
libctf_ast_la_LIBADD += -luuid
endif
if BABELTRACE_BUILD_WITH_LIBC_UUID
libctf_ast_la_LIBADD += -lc
endif
if BABELTRACE_BUILD_WITH_MINGW
libctf_ast_la_LIBADD += -lrpcrt4 -lintl -liconv -lole32 -lpopt
endif

noinst_PROGRAMS = ctf-parser-test
ctf_parser_test_SOURCES = ctf-parser-test.c
ctf_parser_test_CFLAGS = $(AM_CFLAGS) -I$(builddir)
ctf_parser_test_LDADD = \
		libctf-parser.la \
		libctf-ast.la

CLEANFILES = ctf-lexer.c ctf-parser.c ctf-parser.h ctf-parser.output
