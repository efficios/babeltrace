libbabeltrace2_plugin_ctf_lttng_live_la_SOURCES = \
		lttng-live.c \
		lttng-live.h \
		data-stream.c \
		data-stream.h \
		metadata.c \
		metadata.h \
		viewer-connection.c \
		viewer-connection.h \
		lttng-viewer-abi.h

if BABELTRACE_BUILD_WITH_MINGW
libbabeltrace2_plugin_ctf_lttng_live_la_LIBADD = -lws2_32
endif

noinst_LTLIBRARIES = libbabeltrace2-plugin-ctf-lttng-live.la
