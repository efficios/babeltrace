AM_CPPFLAGS += $(PYTHON_INCLUDE)

lib_LTLIBRARIES = libbabeltrace-python-plugin-provider.la

libbabeltrace_python_plugin_provider_la_SOURCES = \
	python-plugin-provider.c
libbabeltrace_python_plugin_provider_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION) \
	$(PYTHON_LIBS)

libbabeltrace_python_plugin_provider_la_LIBADD =

if !ENABLE_BUILT_IN_PLUGINS
libbabeltrace_python_plugin_provider_la_LIBADD += \
	$(top_builddir)/logging/libbabeltrace-logging.la \
	$(top_builddir)/common/libbabeltrace-common.la \
	$(top_builddir)/lib/libbabeltrace.la
endif
