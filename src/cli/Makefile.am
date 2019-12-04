PLUGINS_PATH = $(abs_top_builddir)/src/plugins
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

if ENABLE_PYTHON_PLUGINS
AM_CPPFLAGS += '-DCONFIG_IN_TREE_PROVIDER_DIR="$(abs_top_builddir)/src/python-plugin-provider/.libs"'
endif

bin_PROGRAMS = babeltrace2.bin
noinst_PROGRAMS = babeltrace2

babeltrace2_bin_SOURCES = \
	babeltrace2.c \
	babeltrace2-cfg.c \
	babeltrace2-cfg.h \
	babeltrace2-cfg-cli-args.c \
	babeltrace2-cfg-cli-args.h \
	babeltrace2-cfg-cli-args-connect.c \
	babeltrace2-cfg-cli-args-connect.h \
	babeltrace2-cfg-cli-args-default.h \
	babeltrace2-cfg-cli-args-default.c \
	babeltrace2-log-level.c \
	babeltrace2-log-level.h \
	babeltrace2-plugins.c \
	babeltrace2-plugins.h \
	babeltrace2-query.c \
	babeltrace2-query.h \
	logging.c \
	logging.h

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
babeltrace2_bin_LDFLAGS = $(LD_NO_AS_NEEDED)

# Add all the convenience libraries used by Babeltrace plugins and the
# library. They will be used when embedding plugins (--enable-built-in-plugins),
# otherwise we're looking after multiple definitions of the same symbols if
# a plugin's archive (.a) includes the convenience library because
# we're using --whole-archive below (needed to make sure the linker does
# not discard the plugins since the CLI does not use their symbols
# directly).
babeltrace2_bin_LDADD = \
	$(top_builddir)/src/argpar/libargpar.la \
	$(top_builddir)/src/autodisc/libbabeltrace2-autodisc.la \
	$(top_builddir)/src/param-parse/libbabeltrace2-param-parse.la \
	$(top_builddir)/src/string-format/libbabeltrace2-string-format.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/compat/libcompat.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/ctfser/libbabeltrace2-ctfser.la

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
babeltrace2_bin_LDFLAGS += $(call pluginarchive,ctf)
babeltrace2_bin_LDFLAGS += $(call pluginarchive,text)
babeltrace2_bin_LDFLAGS += $(call pluginarchive,utils)

if ENABLE_DEBUG_INFO
babeltrace2_bin_LDFLAGS += $(call pluginarchive,lttng-utils)
babeltrace2_bin_LDADD += $(ELFUTILS_LIBS)
endif
endif

if BABELTRACE_BUILD_WITH_MINGW
babeltrace2_bin_LDADD += -lws2_32 -lrpcrt4 -lintl -liconv -lole32 -lpthread
endif

# Only used for in-tree execution and tests
babeltrace2_SOURCES = $(babeltrace2_bin_SOURCES)
babeltrace2_LDFLAGS = $(babeltrace2_bin_LDFLAGS)
babeltrace2_LDADD = $(babeltrace2_bin_LDADD)
babeltrace2_CFLAGS = $(AM_CFLAGS) -DBT_SET_DEFAULT_IN_TREE_CONFIGURATION
