if ENABLE_PYTHON_PLUGINS
AM_CPPFLAGS += $(PYTHON_INCLUDE)

pluginproviderdir = "$(BABELTRACE_PLUGIN_PROVIDERS_DIR)"
pluginprovider_LTLIBRARIES = babeltrace2-python-plugin-provider.la

babeltrace2_python_plugin_provider_la_SOURCES = \
	python-plugin-provider.c \
	python-plugin-provider.h

babeltrace2_python_plugin_provider_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-avoid-version -module \
	$(PYTHON_LDFLAGS)

babeltrace2_python_plugin_provider_la_LIBADD =

# Link the Python plugin provider library with libbabeltrace2
# when it's not built-in the babeltrace2 executable.
if !ENABLE_BUILT_IN_PLUGINS
babeltrace2_python_plugin_provider_la_LIBADD += \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/py-common/libbabeltrace2-py-common.la \
	$(top_builddir)/src/lib/libbabeltrace2.la
endif
endif # ENABLE_PYTHON_PLUGINS
