AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -lpopt

bin_PROGRAMS = babeltrace babeltrace-log

babeltrace_SOURCES = \
	babeltrace.c

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code. Note: we put plugin dependencies on the
# executable rather than the library because libtool does not seem to
# issue the LDFLAGS before the rpaths, which makes it useless.
babeltrace_LDFLAGS = -Wl,--no-as-needed
babeltrace_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/libbabeltrace_registry.la \
	$(top_builddir)/formats/ctf/libctf.la \
	$(top_builddir)/formats/ctf/metadata/libctf-ast.la \
	$(top_builddir)/formats/ctf-text/libctf-text.la \
	$(top_builddir)/formats/bt-dummy/libbt-dummy.la

babeltrace_log_SOURCES = babeltrace-log.c

babeltrace_log_LDADD = \
	$(top_builddir)/formats/libbabeltrace_registry.la \
	$(top_builddir)/formats/ctf/libctf.la
