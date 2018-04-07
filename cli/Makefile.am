PLUGINS_PATH = $(abs_top_builddir)/plugins
LTTNG_UTILS_PLUGIN_PATH =

if ENABLE_DEBUG_INFO
LTTNG_UTILS_PLUGIN_PATH += :$(PLUGINS_PATH)/lttng-utils
endif

if BABELTRACE_BUILD_WITH_MINGW
IN_TREE_PLUGIN_PATH := $(shell cygpath -pm "$(PLUGINS_PATH)/ctf:$(PLUGINS_PATH)/text:$(PLUGINS_PATH)/utils$(LTTNG_UTILS_PLUGIN_PATH)")
else
IN_TREE_PLUGIN_PATH = $(PLUGINS_PATH)/ctf:$(PLUGINS_PATH)/text:$(PLUGINS_PATH)/utils$(LTTNG_UTILS_PLUGIN_PATH)
endif

AM_CPPFLAGS += '-DCONFIG_IN_TREE_PLUGIN_PATH="$(IN_TREE_PLUGIN_PATH)"'

bin_PROGRAMS = babeltrace.bin babeltrace-log.bin
noinst_PROGRAMS = babeltrace babeltrace-log

babeltrace_bin_SOURCES = \
	babeltrace.c \
	babeltrace-cfg.c \
	babeltrace-cfg.h \
	babeltrace-cfg-cli-args.c \
	babeltrace-cfg-cli-args.h \
	babeltrace-cfg-cli-args-connect.c \
	babeltrace-cfg-cli-args-connect.h \
	babeltrace-cfg-cli-args-default.h \
	babeltrace-cfg-cli-args-default.c \
	logging.c logging.h

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
babeltrace_bin_LDFLAGS = $(LD_NO_AS_NEEDED)

# Add all the convenience libraries used by Babeltrace plugins and the
# library. They will be used when embedding plugins (--enable-built-in-plugins),
# otherwise we're looking after multiple definitions of the same symbols if
# a plugin's archive (.a) includes the convenience library because
# we're using --whole-archive below (needed to make sure the linker does
# not discard the plugins since the CLI does not use their symbols
# directly).
babeltrace_bin_LDADD = \
	$(top_builddir)/lib/libbabeltrace.la \
	$(top_builddir)/compat/libcompat.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(POPT_LIBS)

if ENABLE_BUILT_IN_PLUGINS
# Takes a plugin name and outputs the needed LDFLAGS to embed it.
#
# The --whole-archive option is important here. From the GNU linker's
# documentation:
#
#     For each archive mentioned on the command line after the
#     --whole-archive option, include every object file in the archive in
#     the link, rather than searching the archive for the required object
#     files.
#
# In our case, we find the plugins thanks to special sections in the
# binary that are filled by plugin objects. If the linker discards those
# symbols because the CLI does not use them directly, the CLI reports
# no plugins found (plugins are effectively not embedded).
pluginarchive = $(LD_WHOLE_ARCHIVE)$(PLUGINS_PATH)/$(1)/.libs/babeltrace-plugin-$(1).a$(LD_NO_WHOLE_ARCHIVE)

# Built-in plugins
babeltrace_bin_LDFLAGS += $(call pluginarchive,ctf)
babeltrace_bin_LDFLAGS += $(call pluginarchive,text)
babeltrace_bin_LDFLAGS += $(call pluginarchive,utils)

if ENABLE_DEBUG_INFO
babeltrace_bin_LDFLAGS += $(call pluginarchive,lttng-utils)
babeltrace_bin_LDADD += $(ELFUTILS_LIBS)
endif
endif

if BABELTRACE_BUILD_WITH_MINGW
babeltrace_bin_LDADD += -lws2_32 -lrpcrt4 -lintl -liconv -lole32 -lpthread
endif

# Only used for in-tree execution and tests
babeltrace_SOURCES = $(babeltrace_bin_SOURCES)
babeltrace_LDFLAGS = $(babeltrace_bin_LDFLAGS)
babeltrace_LDADD = 	$(babeltrace_bin_LDADD)
babeltrace_CFLAGS =	$(AM_CFLAGS) -DBT_SET_DEFAULT_IN_TREE_CONFIGURATION

# babeltrace-log rules and config below
babeltrace_log_bin_SOURCES = babeltrace-log.c
babeltrace_log_bin_LDADD = \
	$(top_builddir)/compat/libcompat.la \
	$(POPT_LIBS)
babeltrace_log_bin_CFLAGS = $(AM_CFLAGS) '-DBT_CLI_PATH="$(abs_top_builddir)/cli/babeltrace$(EXEEXT)"'

# Only used for in-tree execution and tests
babeltrace_log_SOURCES = $(babeltrace_log_bin_SOURCES)
babeltrace_log_LDADD = 	$(babeltrace_log_bin_LDADD)
babeltrace_log_CFLAGS =	$(AM_CFLAGS) '-DBT_CLI_PATH="$(bindir)/babeltrace$(EXEEXT)"'
