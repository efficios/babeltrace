AM_CPPFLAGS += $(PYTHON_INCLUDE)

lib_LTLIBRARIES = libbabeltrace2-python-plugin-provider.la

libbabeltrace2_python_plugin_provider_la_SOURCES = \
	python-plugin-provider.c
libbabeltrace2_python_plugin_provider_la_LDFLAGS = \
	$(LT_NO_UNDEFINED) \
	-version-info $(BABELTRACE_LIBRARY_VERSION) \
	$(PYTHON_LIBS)

libbabeltrace2_python_plugin_provider_la_LIBADD =

if !ENABLE_BUILT_IN_PLUGINS
libbabeltrace2_python_plugin_provider_la_LIBADD += \
	$(top_builddir)/logging/libbabeltrace2-logging.la \
	$(top_builddir)/common/libbabeltrace2-common.la \
	$(top_builddir)/lib/libbabeltrace2.la
endif
