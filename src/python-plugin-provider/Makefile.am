if ENABLE_PYTHON_PLUGINS
AM_CPPFLAGS += $(PYTHON_INCLUDE)

lib_LTLIBRARIES = libbabeltrace2-python-plugin-provider.la

libbabeltrace2_python_plugin_provider_la_SOURCES = \
	python-plugin-provider.c \
	python-plugin-provider.h

libbabeltrace2_python_plugin_provider_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION) \
	$(PYTHON_LDFLAGS)

libbabeltrace2_python_plugin_provider_la_LIBADD =

# Link the Python plugin provider library with libbabeltrace2
# when it's not built-in the babeltrace2 executable.
if !ENABLE_BUILT_IN_PLUGINS
libbabeltrace2_python_plugin_provider_la_LIBADD += \
	$(top_builddir)/src/logging/libbabeltrace2-logging.la \
	$(top_builddir)/src/common/libbabeltrace2-common.la \
	$(top_builddir)/src/py-common/libbabeltrace2-py-common.la \
	$(top_builddir)/src/lib/libbabeltrace2.la
endif
endif # ENABLE_PYTHON_PLUGINS
