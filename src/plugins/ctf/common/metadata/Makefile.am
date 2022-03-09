# SPDX-License-Identifier: MIT

AM_CPPFLAGS += -I$(builddir) -I$(srcdir)
AM_YFLAGS = -t -d -v -Wno-yacc

noinst_LTLIBRARIES = libctf-parser.la libctf-ast.la

BUILT_SOURCES = parser.hpp

libctf_parser_la_SOURCES = lexer.lpp parser.ypp objstack.cpp
# scanner-symbols.h is included to prefix generated yy_* symbols
# with bt_.
libctf_parser_la_CPPFLAGS = $(AM_CPPFLAGS) \
		-include $(srcdir)/scanner-symbols.hpp

# This library contains (mostly) generated code, silence some warnings that it
# produces.
libctf_parser_la_CXXFLAGS = $(AM_CXXFLAGS) \
	-Wno-unused-function \
	-Wno-null-dereference \
	-Wno-missing-field-initializers \
	-Wno-unused-parameter

libctf_ast_la_SOURCES = \
	visitor-generate-ir.cpp \
	visitor-semantic-validator.cpp \
	visitor-parent-links.cpp \
	ast.hpp \
	objstack.hpp \
	parser.hpp \
	parser-wrap.hpp \
	scanner.hpp \
	scanner-symbols.hpp \
	decoder.cpp \
	decoder.hpp \
	decoder-packetized-file-stream-to-buf.cpp \
	decoder-packetized-file-stream-to-buf.hpp \
	logging.cpp \
	logging.hpp \
	ctf-meta.hpp \
	ctf-meta-visitors.hpp \
	ctf-meta-validate.cpp \
	ctf-meta-update-meanings.cpp \
	ctf-meta-update-in-ir.cpp \
	ctf-meta-update-default-clock-classes.cpp \
	ctf-meta-update-text-array-sequence.cpp \
	ctf-meta-update-alignments.cpp \
	ctf-meta-update-value-storing-indexes.cpp \
	ctf-meta-update-stream-class-config.cpp \
	ctf-meta-warn-meaningless-header-fields.cpp \
	ctf-meta-translate.cpp \
	ctf-meta-resolve.cpp \
	ctf-meta-configure-ir-trace.cpp \
	ctf-meta-configure-ir-trace.hpp

if BABELTRACE_BUILD_WITH_MINGW
libctf_ast_la_LIBADD = -lintl -liconv -lole32
endif

# start with empty files to clean
CLEANFILES =

if HAVE_BISON
# we have bison: we can clean the generated parser files
CLEANFILES += parser.cpp parser.hpp parser.output
else # HAVE_BISON
# create target used to stop the build if we want to build the parser,
# but we don't have the necessary tool to do so
ERR_MSG = "Error: Cannot build target because bison is missing."
ERR_MSG += "Make sure bison is installed and run the configure script again."

parser.cpp parser.hpp: parser.ypp
	@echo $(ERR_MSG)
	@false

all-local: parser.cpp parser.hpp
endif # HAVE_BISON

if HAVE_FLEX
# we have flex: we can clean the generated lexer files
CLEANFILES += lexer.cpp
else # HAVE_FLEX
# create target used to stop the build if we want to build the lexer,
# but we don't have the necessary tool to do so
ERR_MSG = "Error: Cannot build target because flex is missing."
ERR_MSG += "Make sure flex is installed and run the configure script again."

filter-lexer.cpp: lexer.lpp
	@echo $(ERR_MSG)
	@false

all-local: lexer.cpp
endif # HAVE_FLEX
