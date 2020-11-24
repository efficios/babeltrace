if ENABLE_RUBY_PLUGINS
AM_CPPFLAGS += $(RUBY_CFLAGS)
AM_CFLAGS += -Wno-strict-prototypes -Wno-nested-externs -Wno-redundant-decls

pluginproviderdir = "$(BABELTRACE_PLUGIN_PROVIDERS_DIR)"
pluginprovider_LTLIBRARIES = babeltrace2-ruby-plugin-provider.la

babeltrace2_ruby_plugin_provider_la_SOURCES = \
	ruby-plugin-provider.c \
	ruby-plugin-provider.h

babeltrace2_ruby_plugin_provider_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module \
	$(RUBY_LIBS)

babeltrace2_ruby_plugin_provider_la_LIBADD =

# Link the Python plugin provider library with libbabeltrace2
# when it's not built-in the babeltrace2 executable.
if !ENABLE_BUILT_IN_PLUGINS
babeltrace2_ruby_plugin_provider_la_LIBADD += \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/lib/libbabeltrace2.la
endif
endif # ENABLE_RUBY_PLUGINS
