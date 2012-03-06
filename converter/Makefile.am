AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS = -lpopt

bin_PROGRAMS = babeltrace babeltrace-log

babeltrace_SOURCES = \
	babeltrace.c

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
babeltrace_LDFLAGS = -Wl,--no-as-needed
babeltrace_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/formats/ctf-text/libbabeltrace-ctf-text.la \
	$(top_builddir)/formats/bt-dummy/libbabeltrace-dummy.la

babeltrace_log_SOURCES = babeltrace-log.c

babeltrace_log_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la

if BABELTRACE_BUILD_WITH_LIBUUID
babeltrace_log_LDADD += -luuid
endif
if BABELTRACE_BUILD_WITH_LIBC_UUID
babeltrace_log_LDADD += -lc
endif
