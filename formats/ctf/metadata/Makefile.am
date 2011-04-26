AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include
BUILT_SOURCES = ctf-parser.h
AM_YFLAGS = -t -d

noinst_LIBRARIES = libctf-parser.a libctf-ast.a

libctf_parser_a_SOURCES = ctf-lexer.l ctf-parser.y

libctf_ast_a_SOURCES = ctf-visitor-xml.c \
		ctf-visitor-parent-links.c \
		ctf-visitor-semantic-validator.c \
		ctf-visitor-generate-io-struct.c

bin_PROGRAMS = ctf-parser-test
ctf_parser_test_SOURCES = ctf-parser-test.c
ctf_parser_test_LDADD = libctf-parser.a \
		libctf-ast.a \
		$(top_builddir)/types/libtypes.a
