AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include
BUILT_SOURCES = ctf-parser.h
AM_YFLAGS = -t -d

noinst_LIBRARIES = libctf-parser.a

libctf_parser_a_SOURCES = ctf-lexer.l ctf-parser.y

bin_PROGRAMS = ctf-parser-test
ctf_parser_test_SOURCES = \
			ctf-parser-test.c ctf-visitor-xml.c \
			ctf-lexer.l ctf-parser.y
