AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include

libbabeltrace_plugin_ctf_lttng_live_la_SOURCES = lttng-live.c

noinst_HEADERS = lttng-live-internal.h
noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-lttng-live.la

# Request that the linker keeps all static library objects.
libbabeltrace_plugin_ctf_lttng_live_la_LDFLAGS = \
	$(LD_NO_AS_NEEDED)

libbabeltrace_plugin_ctf_lttng_live_la_LIBADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/formats/ctf/libbabeltrace-ctf.la \
	$(top_builddir)/plugins/libbabeltrace-plugin.la
