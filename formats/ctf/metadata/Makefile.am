AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include
BUILT_SOURCES = ctf-parser.h
AM_YFLAGS = -t -d -v

lib_LTLIBRARIES = libctf-parser.la libctf-ast.la

noinst_HEADERS = \
	ctf-scanner.h \
	ctf-ast.h

libctf_parser_la_SOURCES = ctf-lexer.l ctf-parser.y

libctf_ast_la_SOURCES = ctf-visitor-xml.c \
		ctf-visitor-parent-links.c \
		ctf-visitor-semantic-validator.c \
		ctf-visitor-generate-io-struct.c

libctf_ast_la_LIBADD = \
		 $(top_builddir)/types/libbabeltrace_types.la

noinst_PROGRAMS = ctf-parser-test
ctf_parser_test_SOURCES = ctf-parser-test.c

ctf_parser_test_LDADD = \
		libctf-parser.la \
		libctf-ast.la
