AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include -I$(top_srcdir)/plugins

libbabeltrace_plugin_ctf_lttng_live_la_SOURCES = lttng-live.c \
		data-stream.c lttng-live-internal.h \
		data-stream.h metadata.c metadata.h \
		viewer-connection.c viewer-connection.h \
		lttng-viewer-abi.h

noinst_LTLIBRARIES = libbabeltrace-plugin-ctf-lttng-live.la
