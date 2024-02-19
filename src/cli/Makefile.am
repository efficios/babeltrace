# SPDX-FileCopyrightText: 2019-2023 EfficiOS, Inc.
# SPDX-License-Identifier: MIT

include $(top_srcdir)/src/Makefile.common.inc

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
	logging.cpp \
	logging.h

# -Wl,--no-as-needed is needed for recent gold linker who seems to think
# it knows better and considers libraries with constructors having
# side-effects as dead code.
babeltrace2_bin_LDFLAGS = $(AM_LDFLAGS) $(LD_NO_AS_NEEDED)

EXTRA_babeltrace2_bin_DEPENDENCIES =

# Add all the convenience libraries used by Babeltrace plugins and the
# library. They will be used when embedding plugins (--enable-built-in-plugins),
# otherwise we're looking after multiple definitions of the same symbols if
# a plugin's archive (.a) includes the convenience library because
# we're using --whole-archive below (needed to make sure the linker does
# not discard the plugins since the CLI does not use their symbols
# directly).
babeltrace2_bin_LDADD = \
	$(top_builddir)/src/argpar/libargpar.la \
	$(top_builddir)/src/autodisc/libautodisc.la \
	$(top_builddir)/src/param-parse/libparam-parse.la \
	$(top_builddir)/src/string-format/libstring-format.la \
	$(top_builddir)/src/lib/libbabeltrace2.la \
	$(top_builddir)/src/compat/libcompat.la \
	$(top_builddir)/src/common/libcommon.la \
	$(top_builddir)/src/cpp-common/vendor/fmt/libfmt.la \
	$(top_builddir)/src/logging/liblogging.la \
	$(top_builddir)/src/ctfser/libctfser.la

if ENABLE_BUILT_IN_PLUGINS

# Built-in plugins
babeltrace2_bin_LDFLAGS += $(call pluginarchive,ctf)
babeltrace2_bin_LDFLAGS += $(call pluginarchive,text)
babeltrace2_bin_LDFLAGS += $(call pluginarchive,utils)

EXTRA_babeltrace2_bin_DEPENDENCIES += \
	$(PLUGINS_PATH)/ctf/babeltrace-plugin-ctf.la \
	$(PLUGINS_PATH)/text/babeltrace-plugin-text.la \
	$(PLUGINS_PATH)/utils/babeltrace-plugin-utils.la

if ENABLE_DEBUG_INFO
babeltrace2_bin_LDFLAGS += $(call pluginarchive,lttng-utils)
babeltrace2_bin_LDADD += $(ELFUTILS_LIBS)

EXTRA_babeltrace2_bin_DEPENDENCIES += \
	$(PLUGINS_PATH)/lttng-utils/babeltrace-plugin-lttng-utils.la
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
EXTRA_babeltrace2_DEPENDENCIES = $(EXTRA_babeltrace2_bin_DEPENDENCIES)
