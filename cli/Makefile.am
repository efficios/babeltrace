PLUGINS_PATH = $(abs_top_builddir)/plugins
AM_CFLAGS = $(PACKAGE_CFLAGS) -I$(top_srcdir)/include \
		-DCONFIG_IN_TREE_PLUGIN_PATH=\"$(PLUGINS_PATH)/ctf:$(PLUGINS_PATH)/lttng-utils:$(PLUGINS_PATH)/text:$(PLUGINS_PATH)/writer:$(PLUGINS_PATH)/utils\"
AM_LDFLAGS = -lpopt

bin_PROGRAMS = babeltrace.bin
noinst_PROGRAMS = babeltrace
#check_PROGRAMS = babeltrace

babeltrace_bin_SOURCES = \
	babeltrace.c \
	babeltrace-cfg.c \
	babeltrace-cfg.h \
	babeltrace-cfg-cli-args.c \
	babeltrace-cfg-cli-args.h \
	babeltrace-cfg-cli-args-connect.c \
	babeltrace-cfg-cli-args-connect.h \
	babeltrace-cfg-cli-args-default.h \
	babeltrace-cfg-cli-args-default.c

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
babeltrace_bin_LDFLAGS = -Wl, $(LD_NO_AS_NEEDED), -export-dynamic
babeltrace_bin_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/compat/libcompat.la \
	$(top_builddir)/common/libbabeltrace-common.la

if BUILT_IN_PLUGINS
babeltrace_bin_LDFLAGS += -Wl,--whole-archive,$(top_builddir)/plugins/ctf/.libs/libbabeltrace-plugin-ctf.a,$(top_builddir)/plugins/text/.libs/libbabeltrace-plugin-ctf-text.a,$(top_builddir)/plugins/muxer/.libs/libbabeltrace-plugin-muxer.a,$(top_builddir)/plugins/writer/.libs/libbabeltrace-plugin-ctf-writer.a,--no-whole-archive
endif

if BABELTRACE_BUILD_WITH_MINGW
babeltrace_bin_LDADD += -lrpcrt4 -lintl -liconv -lole32 -lpopt -lpthread
endif

# Only used for in-tree execution and tests
babeltrace_SOURCES = $(babeltrace_bin_SOURCES)
babeltrace_LDFLAGS = $(babeltrace_bin_LDFLAGS)
babeltrace_LDADD = 	$(babeltrace_bin_LDADD)
babeltrace_CFLAGS =	$(AM_CFLAGS) -DBT_SET_DEFAULT_IN_TREE_CONFIGURATION
