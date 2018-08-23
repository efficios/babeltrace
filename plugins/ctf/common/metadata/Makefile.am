AM_CPPFLAGS += -I$(builddir) -I$(srcdir)
AM_YFLAGS = -t -d -v

noinst_LTLIBRARIES = libctf-parser.la libctf-ast.la

BUILT_SOURCES = parser.h

libctf_parser_la_SOURCES = lexer.l parser.y objstack.c
# scanner-symbols.h is included to prefix generated yy_* symbols
# with bt_.
libctf_parser_la_CPPFLAGS = $(AM_CPPFLAGS) \
		-include $(srcdir)/scanner-symbols.h
libctf_parser_la_CFLAGS = $(AM_CFLAGS) -Wno-unused-function

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
	decoder.h \
	logging.c \
	logging.h \
	ctf-meta.h \
	ctf-meta-visitors.h \
	ctf-meta-validate.c \
	ctf-meta-update-meanings.c \
	ctf-meta-update-in-ir.c \
	ctf-meta-update-default-clock-classes.c \
	ctf-meta-update-text-array-sequence.c \
	ctf-meta-update-value-storing-indexes.c \
	ctf-meta-translate.c \
	ctf-meta-resolve.c

libctf_ast_la_LIBADD = $(UUID_LIBS)

if BABELTRACE_BUILD_WITH_MINGW
libctf_ast_la_LIBADD += -lrpcrt4 -lintl -liconv -lole32 $(POPT_LIBS)
endif

# start with empty files to clean
CLEANFILES =

if HAVE_BISON
# we have bison: we can clean the generated parser files
CLEANFILES += parser.c parser.h parser.output
else # HAVE_BISON
# create target used to stop the build if we want to build the parser,
# but we don't have the necessary tool to do so
ERR_MSG = "Error: Cannot build target because bison is missing."
ERR_MSG += "Make sure bison is installed and run the configure script again."

parser.c parser.h: parser.y
	@echo $(ERR_MSG)
	@false

all-local: parser.c parser.h
endif # HAVE_BISON

if HAVE_FLEX
# we have flex: we can clean the generated lexer files
CLEANFILES += lexer.c
else # HAVE_FLEX
# create target used to stop the build if we want to build the lexer,
# but we don't have the necessary tool to do so
ERR_MSG = "Error: Cannot build target because flex is missing."
ERR_MSG += "Make sure flex is installed and run the configure script again."

filter-lexer.c: lexer.l
	@echo $(ERR_MSG)
	@false

all-local: lexer.c
endif # HAVE_FLEX
