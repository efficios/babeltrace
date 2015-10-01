AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include
AM_LDFLAGS = -lpopt

bin_PROGRAMS = babeltrace babeltrace-log

babeltrace_SOURCES = \
	babeltrace.c

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
babeltrace_LDFLAGS = $(LD_NO_AS_NEEDED)
babeltrace_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/compat/libcompat.la \
	$(top_builddir)/formats/ctf-text/libbabeltrace-ctf-text.la \
	$(top_builddir)/formats/ctf-metadata/libbabeltrace-ctf-metadata.la \
	$(top_builddir)/formats/bt-dummy/libbabeltrace-dummy.la \
	$(top_builddir)/formats/lttng-live/libbabeltrace-lttng-live.la

babeltrace_log_SOURCES = babeltrace-log.c

babeltrace_log_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/compat/libcompat.la

if BABELTRACE_BUILD_WITH_LIBUUID
babeltrace_log_LDADD += -luuid
endif
if BABELTRACE_BUILD_WITH_LIBC_UUID
babeltrace_log_LDADD += -lc
endif

if BABELTRACE_BUILD_WITH_MINGW
babeltrace_log_LDADD += -lrpcrt4 -lintl -liconv -lole32 -lpopt -lpthread
babeltrace_LDADD += -lrpcrt4 -lintl -liconv -lole32 -lpopt -lpthread
endif
